#include "output/edid.h"

#include "wlr_color.h"

#include <memory>

extern "C" {
#include <libdisplay-info/edid.h>
#include <libdisplay-info/info.h>
}

namespace umbriel {

  std::optional<int> edidMinRefreshHz(std::span<const uint8_t> edid) {
    if (edid.empty()) {
      return std::nullopt;
    }
    std::unique_ptr<di_info, decltype(&di_info_destroy)> info(
        di_info_parse_edid(edid.data(), edid.size()), di_info_destroy
    );
    const di_edid* parsed = info ? di_info_get_edid(info.get()) : nullptr;
    if (parsed == nullptr) {
      return std::nullopt;
    }
    for (const di_edid_display_descriptor* const* descriptor = di_edid_get_display_descriptors(parsed);
         *descriptor != nullptr; ++descriptor) {
      const di_edid_display_range_limits* limits = di_edid_display_descriptor_get_range_limits(*descriptor);
      if (limits != nullptr && limits->min_vert_rate_hz > 0) {
        return static_cast<int>(limits->min_vert_rate_hz);
      }
    }
    return std::nullopt;
  }

} // namespace umbriel
