#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct wlr_output;

namespace umbriel {

  // Read the EDID attached to a native DRM output. Empty for nested and virtual outputs and unreadable EDIDs.
  [[nodiscard]] std::vector<uint8_t> outputEdid(wlr_output* output);

  // The lowest vertical refresh rate in Hz the sink's display range limits allow, which is the bottom of its
  // adaptive-sync range. No value when the EDID does not publish range limits.
  [[nodiscard]] std::optional<int> edidMinRefreshHz(std::span<const uint8_t> edid);

} // namespace umbriel
