#include "check.h"
#include "output/frame_schedule.h"

using umbriel::OutputFrameFollowup;
using umbriel::outputFrameFollowup;
using umbriel::TearingCommitRecovery;
using umbriel::vrrFloorDelayMs;
using umbriel::vrrFloorHz;

UMBRIEL_TEST(nestedBackendCanScheduleFrames) {
  CHECK_EQ(outputFrameFollowup(false, nullptr, false, true), OutputFrameFollowup::Schedule);
}

UMBRIEL_TEST(activeNativeSessionCanScheduleFrames) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(false, &session, false, true), OutputFrameFollowup::Schedule);
}

UMBRIEL_TEST(inactiveNativeSessionCannotScheduleFrames) {
  wlr_session session{};
  session.active = false;
  CHECK_EQ(outputFrameFollowup(false, &session, false, true), OutputFrameFollowup::None);
}

UMBRIEL_TEST(stoppingCompositorCannotScheduleFrames) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(true, &session, false, true), OutputFrameFollowup::None);
}

UMBRIEL_TEST(failedCommitUsesDelayedRetry) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(false, &session, true, false), OutputFrameFollowup::RetryDelayed);
}

UMBRIEL_TEST(failedAnimatedFrameDoesNotScheduleImmediately) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(false, &session, true, true), OutputFrameFollowup::RetryDelayed);
}

UMBRIEL_TEST(idleSuccessfulFrameNeedsNoFollowup) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(false, &session, false, false), OutputFrameFollowup::None);
}

UMBRIEL_TEST(failedTearingCommitForcesARegularRecoveryCommit) {
  TearingCommitRecovery recovery;

  CHECK(recovery.requestTearing(true));
  recovery.recordCommit(true, false);

  CHECK(recovery.regularCommitPending());
  CHECK(!recovery.requestTearing(true));
}

UMBRIEL_TEST(failedRegularRecoveryStaysRegularUntilItSucceeds) {
  TearingCommitRecovery recovery;
  recovery.recordCommit(true, false);
  recovery.recordCommit(false, false);

  CHECK(recovery.regularCommitPending());
  CHECK(!recovery.requestTearing(true));

  recovery.recordCommit(false, true);
  CHECK(!recovery.regularCommitPending());
  CHECK(recovery.requestTearing(true));
}

UMBRIEL_TEST(rejectedTearingTestKeepsFailedRegularFallbackOnTheRegularPath) {
  TearingCommitRecovery recovery;
  recovery.forceRegularCommit();
  recovery.recordCommit(false, false);

  CHECK(recovery.regularCommitPending());
  CHECK(!recovery.requestTearing(true));

  recovery.recordCommit(false, true);
  CHECK(recovery.requestTearing(true));
}

UMBRIEL_TEST(ineligibleFramesNeverRequestTearing) {
  TearingCommitRecovery recovery;
  CHECK(!recovery.requestTearing(false));
  recovery.recordCommit(false, true);
  CHECK(!recovery.regularCommitPending());
}

UMBRIEL_TEST(effectFrameDelayFollowsRefreshOrCapsAtMaxFps) {
  CHECK_EQ(umbriel::effectFrameDelayMs(0, 1000, 990), uint64_t{0});
  CHECK_EQ(umbriel::effectFrameDelayMs(60, 1000, 1000), uint64_t{17});
  CHECK_EQ(umbriel::effectFrameDelayMs(60, 1010, 1000), uint64_t{7});
  CHECK_EQ(umbriel::effectFrameDelayMs(60, 1100, 1000), uint64_t{1});
  CHECK_EQ(umbriel::effectFrameDelayMs(1, 1000, 1000), uint64_t{1000});
}

UMBRIEL_TEST(effectFrameDelayWaitsAFullIntervalWhenTheLastFrameIsAhead) {
  CHECK_EQ(umbriel::effectFrameDelayMs(60, 990, 1000), uint64_t{17});
}

UMBRIEL_TEST(effectFrameDelayNeverDropsBelowOneMillisecond) {
  CHECK_EQ(umbriel::effectFrameDelayMs(2000, 1000, 1000), uint64_t{1});
}

int main() { return RUN_TESTS(); }

UMBRIEL_TEST(vrrFloorIsOffWithoutAMinimum) {
  CHECK_EQ(vrrFloorDelayMs(0), uint64_t{0});
  CHECK_EQ(vrrFloorDelayMs(-1), uint64_t{0});
}

UMBRIEL_TEST(vrrFloorFiresBeforeTheFloorPeriodEnds) {
  // 100 Hz is a 10 ms period: one millisecond stays free for the render and commit.
  CHECK_EQ(vrrFloorDelayMs(100), uint64_t{9});
  // 60 Hz is 16.67 ms: rounded down, then the render millisecond.
  CHECK_EQ(vrrFloorDelayMs(60), uint64_t{15});
  CHECK_EQ(vrrFloorDelayMs(144), uint64_t{5});
}

UMBRIEL_TEST(vrrFloorNeverSpinsAtVeryHighRates) {
  CHECK_EQ(vrrFloorDelayMs(500), uint64_t{1});
  CHECK_EQ(vrrFloorDelayMs(1000), uint64_t{1});
}

UMBRIEL_TEST(vrrFloorNeverExceedsTheModeRefresh) {
  CHECK_EQ(vrrFloorHz(100, 48, 165000), 100);
  CHECK_EQ(vrrFloorHz(200, 48, 165000), 165);
  // A 165 Hz mode reporting 164.999 Hz still caps at 165.
  CHECK_EQ(vrrFloorHz(200, 48, 164999), 165);
  CHECK_EQ(vrrFloorHz(144, 0, 0), 144);
}

UMBRIEL_TEST(vrrFloorNeverDropsBelowTheMonitorRange) {
  CHECK_EQ(vrrFloorHz(30, 48, 165000), 48);
  CHECK_EQ(vrrFloorHz(30, 0, 165000), 30);
  // A range that claims more than the mode still yields to the mode.
  CHECK_EQ(vrrFloorHz(30, 200, 165000), 165);
}

UMBRIEL_TEST(vrrFloorStaysOffWithoutAConfiguredMinimum) { CHECK_EQ(vrrFloorHz(0, 48, 165000), 0); }
