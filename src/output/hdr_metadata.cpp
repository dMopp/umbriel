#include "output/hdr_metadata.h"

#include <algorithm>
#include <cmath>
#include <memory>

extern "C" {
#include <libdisplay-info/info.h>
}

namespace umbriel {

  namespace {

    double finiteRange(float value, double maximum) {
      return std::isfinite(value) ? std::clamp(static_cast<double>(value), 0.0, maximum) : 0.0;
    }

    bool validCoordinate(const wlr_color_cie1931_xy& coordinate) {
      return std::isfinite(coordinate.x)
          && std::isfinite(coordinate.y)
          && coordinate.x >= 0.0
          && coordinate.y >= 0.0
          && coordinate.x + coordinate.y <= 1.0;
    }

    bool validPrimaries(const wlr_color_primaries& primaries) {
      if (!validCoordinate(primaries.red)
          || !validCoordinate(primaries.green)
          || !validCoordinate(primaries.blue)
          || !validCoordinate(primaries.white)) {
        return false;
      }
      const double twiceArea = std::abs(
          (primaries.green.x - primaries.red.x) * (primaries.blue.y - primaries.red.y)
          - (primaries.blue.x - primaries.red.x) * (primaries.green.y - primaries.red.y)
      );
      return twiceArea > 0.000001 && primaries.white.x > 0.0 && primaries.white.y > 0.0;
    }

  } // namespace

  std::optional<HdrStaticMetadata> hdrStaticMetadataFromEdid(std::span<const uint8_t> edid) {
    if (edid.empty()) {
      return std::nullopt;
    }
    std::unique_ptr<di_info, decltype(&di_info_destroy)> info(
        di_info_parse_edid(edid.data(), edid.size()), di_info_destroy
    );
    if (!info) {
      return std::nullopt;
    }

    const di_hdr_static_metadata* source = di_info_get_hdr_static_metadata(info.get());
    if (!source->type1 || !source->pq) {
      return std::nullopt;
    }

    HdrStaticMetadata metadata{
        .minLuminance = finiteRange(source->desired_content_min_luminance, 6.5535),
        .maxLuminance = std::round(finiteRange(source->desired_content_max_luminance, 65535.0)),
        .maxFrameAverageLuminance = std::round(finiteRange(source->desired_content_max_frame_avg_luminance, 65535.0)),
    };
    if (metadata.maxLuminance > 0.0) {
      metadata.maxFrameAverageLuminance = std::min(metadata.maxFrameAverageLuminance, metadata.maxLuminance);
    }
    return metadata;
  }

  wlr_output_image_description makeHdrOutputDescription(
      const wlr_color_primaries* displayPrimaries, const std::optional<HdrStaticMetadata>& metadata
  ) {
    wlr_color_primaries targetPrimaries{};
    if (displayPrimaries != nullptr && validPrimaries(*displayPrimaries)) {
      targetPrimaries = *displayPrimaries;
    } else {
      wlr_color_primaries_from_named(&targetPrimaries, WLR_COLOR_NAMED_PRIMARIES_BT2020);
    }

    const double minLuminance = metadata ? metadata->minLuminance : 0.0;
    const double maxLuminance = metadata ? metadata->maxLuminance : 0.0;
    const double maxFrameAverageLuminance = metadata ? metadata->maxFrameAverageLuminance : 0.0;
    return {
        .primaries = WLR_COLOR_NAMED_PRIMARIES_BT2020,
        .transfer_function = WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ,
        .mastering_display_primaries = targetPrimaries,
        .mastering_luminance = {.min = minLuminance, .max = maxLuminance},
        .max_cll = maxLuminance,
        .max_fall = maxFrameAverageLuminance,
    };
  }

} // namespace umbriel
