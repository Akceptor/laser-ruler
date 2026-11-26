#pragma once

#include <cstdint>

// GPIO numbers, not physical header positions. Verify against your exact board.
namespace config {
constexpr int kSda = 0;
constexpr int kScl = 1;
constexpr int kRangeRx = 4;
constexpr int kRangeTx = 5;
constexpr int kBoot = 28;

// Default: SW-SHOT wired to GND per the supplied manual's test procedure.
// Remove that jumper BEFORE selecting a GPIO here. Verify actual polarity first.
constexpr int kEnablePin = -1;
constexpr bool kEnableActiveHigh = false;

constexpr uint8_t kDisplayWidth = 128;
constexpr uint8_t kDisplayHeight = 64; // Change to 32 for a 128x32 OLED.
constexpr uint8_t kDisplayAddress = 0; // 0 = try 0x3C, then 0x3D.
constexpr uint32_t kDisplayIntervalMs = 100;
static_assert(kDisplayWidth == 128 &&
              (kDisplayHeight == 32 || kDisplayHeight == 64),
              "Supported OLED sizes: 128x32 or 128x64");
} // namespace config
