#include "output/edid.h"

#include "wlr_color.h"

#include <memory>

extern "C" {
#include <libdisplay-info/edid.h>
#include <libdisplay-info/info.h>
#if UMBRIEL_HAS_DRM_BACKEND
#include <cstring>
#include <unistd.h>
#include <wlr/backend/drm.h>
#include <wlr/types/wlr_output.h>
#include <xf86drmMode.h>
#endif
}

namespace umbriel {

  namespace {

#if UMBRIEL_HAS_DRM_BACKEND
    struct FdCloser {
      void operator()(int* fd) const {
        if (fd != nullptr) {
          close(*fd);
          delete fd;
        }
      }
    };

    struct ObjectPropertiesDeleter {
      void operator()(drmModeObjectProperties* properties) const { drmModeFreeObjectProperties(properties); }
    };

    struct PropertyDeleter {
      void operator()(drmModePropertyRes* property) const { drmModeFreeProperty(property); }
    };

    struct PropertyBlobDeleter {
      void operator()(drmModePropertyBlobRes* blob) const { drmModeFreePropertyBlob(blob); }
    };
#endif

  } // namespace

  std::vector<uint8_t> outputEdid(wlr_output* output) {
#if UMBRIEL_HAS_DRM_BACKEND
    if (output == nullptr || !wlr_output_is_drm(output)) {
      return {};
    }

    const int rawFd = wlr_drm_backend_get_non_master_fd(output->backend);
    if (rawFd < 0) {
      return {};
    }
    const std::unique_ptr<int, FdCloser> fd(new int(rawFd));
    const uint32_t connectorId = wlr_drm_connector_get_id(output);
    const std::unique_ptr<drmModeObjectProperties, ObjectPropertiesDeleter> properties(
        drmModeObjectGetProperties(*fd, connectorId, DRM_MODE_OBJECT_CONNECTOR)
    );
    if (!properties) {
      return {};
    }

    for (uint32_t index = 0; index < properties->count_props; ++index) {
      const std::unique_ptr<drmModePropertyRes, PropertyDeleter> property(
          drmModeGetProperty(*fd, properties->props[index])
      );
      if (!property
          || (property->flags & DRM_MODE_PROP_BLOB) == 0
          || std::strcmp(property->name, "EDID") != 0
          || properties->prop_values[index] == 0) {
        continue;
      }
      const std::unique_ptr<drmModePropertyBlobRes, PropertyBlobDeleter> blob(
          drmModeGetPropertyBlob(*fd, properties->prop_values[index])
      );
      if (!blob || blob->data == nullptr || blob->length == 0) {
        return {};
      }
      const auto* data = static_cast<const uint8_t*>(blob->data);
      return {data, data + blob->length};
    }
#else
    (void)output;
#endif
    return {};
  }

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
