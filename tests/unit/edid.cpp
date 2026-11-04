#include "output/edid.h"

#include "check.h"

#include <array>
#include <cstdint>

namespace {

  // A base EDID 1.4 block with display range limits in its second descriptor slot. The first slot stays empty and
  // the last two are dummy descriptors, which is enough for libdisplay-info to parse the range.
  std::array<uint8_t, 128> rangeLimitsEdid(uint8_t minVerticalHz, uint8_t maxVerticalHz) {
    std::array<uint8_t, 128> edid{};
    for (size_t index = 1; index < 7; ++index) {
      edid[index] = 0xFF;
    }
    edid[8] = 0x1E;
    edid[9] = 0x6D;
    edid[17] = 30;
    edid[18] = 0x01;
    edid[19] = 0x04;
    edid[20] = 0x80;
    edid[21] = 60;
    edid[22] = 34;
    edid[23] = 120;
    // Continuous frequency, which a GTF range requires.
    edid[24] = 0x0B;
    // Unused standard timings.
    for (size_t index = 38; index < 54; ++index) {
      edid[index] = 0x01;
    }

    constexpr size_t descriptor = 72;
    edid[descriptor + 3] = 0xFD;
    edid[descriptor + 5] = minVerticalHz;
    edid[descriptor + 6] = maxVerticalHz;
    edid[descriptor + 7] = 30;
    edid[descriptor + 8] = 255;
    edid[descriptor + 9] = 220;
    edid[descriptor + 11] = 0x0A;
    for (size_t index = descriptor + 12; index < descriptor + 18; ++index) {
      edid[index] = 0x20;
    }
    edid[90 + 3] = 0x10;
    edid[108 + 3] = 0x10;

    uint8_t checksum = 0;
    for (size_t index = 0; index < 127; ++index) {
      checksum = static_cast<uint8_t>(checksum + edid[index]);
    }
    edid[127] = static_cast<uint8_t>(0 - checksum);
    return edid;
  }

} // namespace

UMBRIEL_TEST(edidRangeLimitsGiveTheMinimumRefresh) {
  const auto edid = rangeLimitsEdid(48, 165);
  const auto minimum = umbriel::edidMinRefreshHz(edid);
  CHECK(minimum.has_value());
  if (minimum) {
    CHECK_EQ(*minimum, 48);
  }
}

UMBRIEL_TEST(edidWithoutRangeLimitsHasNoMinimumRefresh) {
  std::array<uint8_t, 128> edid{};
  CHECK(!umbriel::edidMinRefreshHz(edid).has_value());
  CHECK(!umbriel::edidMinRefreshHz({}).has_value());
}

int main() { return RUN_TESTS(); }
