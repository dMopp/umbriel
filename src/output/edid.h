#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace umbriel {

  // The lowest vertical refresh rate in Hz the sink's display range limits allow, which is the bottom of its
  // adaptive-sync range. No value when the EDID does not publish range limits.
  [[nodiscard]] std::optional<int> edidMinRefreshHz(std::span<const uint8_t> edid);

} // namespace umbriel
