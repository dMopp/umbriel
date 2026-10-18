#pragma once

#include <algorithm>
#include <cstdint>

extern "C" {
#include <wlr/backend/session.h>
}

namespace umbriel {

  enum class OutputFrameFollowup {
    None,
    Schedule,
    RetryDelayed,
  };

  [[nodiscard]] inline bool outputFrameAllowed(bool stopping, const wlr_session* session) {
    return !stopping && (session == nullptr || session->active);
  }

  [[nodiscard]] inline OutputFrameFollowup
  outputFrameFollowup(bool stopping, const wlr_session* session, bool commitFailed, bool animationsActive) {
    if (!outputFrameAllowed(stopping, session)) {
      return OutputFrameFollowup::None;
    }
    if (commitFailed) {
      return OutputFrameFollowup::RetryDelayed;
    }
    return animationsActive ? OutputFrameFollowup::Schedule : OutputFrameFollowup::None;
  }

  // Delay before the next effect-only frame. 0 follows the output's refresh (schedule immediately); otherwise the
  // interval for max_fps minus the time already elapsed, at least 1 ms so a late timer never spins.
  [[nodiscard]] inline uint64_t effectFrameDelayMs(int maxFps, uint64_t nowMsec, uint64_t lastEffectFrameMsec) {
    if (maxFps <= 0) {
      return 0;
    }
    const auto fps = static_cast<uint64_t>(maxFps);
    // Rounded up so the timer path never exceeds max_fps; a frame another source schedules may land up to 1 ms early.
    const uint64_t interval = (1000 + fps - 1) / fps;
    const uint64_t elapsed = nowMsec > lastEffectFrameMsec ? nowMsec - lastEffectFrameMsec : 0;
    return elapsed >= interval ? 1 : std::max<uint64_t>(1, interval - elapsed);
  }

  // The VRR refresh floor in Hz, kept within what the monitor can do: never below the bottom of its refresh range
  // (`monitorMinHz`, from the EDID) and never above the mode's refresh rate, so the floor never asks for more than
  // the configured mode. `modeRefreshMHz` is rounded to the nearest hertz, since modes like 165 Hz often report
  // 164.999. A 0 for either bound means unknown and leaves that side open.
  [[nodiscard]] inline int vrrFloorHz(int configuredHz, int monitorMinHz, int modeRefreshMHz) {
    if (configuredHz <= 0) {
      return 0;
    }
    const int floor = std::max(configuredHz, monitorMinHz);
    if (modeRefreshMHz <= 0) {
      return floor;
    }
    return std::min(floor, (modeRefreshMHz + 500) / 1000);
  }

  // Delay after a presentation before forcing another frame, so adaptive sync never lets the refresh rate fall below
  // `minHz`. The timer counts whole milliseconds, so the period is rounded down and one more millisecond is left for
  // the render and commit that follow. 0 when no floor is set.
  [[nodiscard]] inline uint64_t vrrFloorDelayMs(int minHz) {
    if (minHz <= 0) {
      return 0;
    }
    const uint64_t period = 1000 / static_cast<uint64_t>(minHz);
    return period > 2 ? period - 1 : 1;
  }

  // The latest vblank at or before `nowNs` on the cadence of the last presentation: evenly spaced per output, yet
  // never ahead of now. Projecting forward to the next vblank instead would put a slow output's instant up to a whole
  // period ahead of a fast one's; outputs share one animation state, so the fast output would then repeat frames.
  // 0 when nothing predicts it (no presentation yet, or no fixed refresh), so the caller uses now.
  [[nodiscard]] inline uint64_t latestVblankNs(uint64_t nowNs, uint64_t lastPresentNs, uint64_t refreshNs) {
    if (lastPresentNs == 0 || refreshNs == 0) {
      return 0;
    }
    if (lastPresentNs >= nowNs) {
      return lastPresentNs;
    }
    return lastPresentNs + (nowNs - lastPresentNs) / refreshNs * refreshNs;
  }

  // An asynchronous page flip can pass the backend test and still fail at commit time. The generic frame retry must
  // submit one fresh regular state before another asynchronous attempt, otherwise a backend rejection can become a
  // self-sustaining retry loop.
  class TearingCommitRecovery {
  public:
    [[nodiscard]] bool requestTearing(bool eligible) const { return eligible && !m_regularCommitPending; }
    [[nodiscard]] bool regularCommitPending() const { return m_regularCommitPending; }

    void recordCommit(bool requestedTearing, bool succeeded) {
      if (requestedTearing && !succeeded) {
        m_regularCommitPending = true;
      } else if (!requestedTearing && succeeded) {
        m_regularCommitPending = false;
      }
    }

    void forceRegularCommit() { m_regularCommitPending = true; }

    void reset() { m_regularCommitPending = false; }

  private:
    bool m_regularCommitPending = false;
  };

} // namespace umbriel
