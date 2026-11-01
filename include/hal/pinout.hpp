#pragma once
#include <cstdint>

// Single source of truth for the board wiring (matches README and firmware/src/screensaver.cpp).
namespace phasebox::hal::pins {
constexpr uint8_t ENC_A      = 2;   // rotary encoder phase A
constexpr uint8_t ENC_B      = 3;   // rotary encoder phase B
constexpr uint8_t OLED_SDA   = 4;   // I2C0 SDA
constexpr uint8_t OLED_SCL   = 5;   // I2C0 SCL
constexpr uint8_t ENC_SW     = 6;   // encoder push switch
constexpr uint8_t FS_TAP     = 10;  // footswitch 1 (tap / phase reset)
constexpr uint8_t FS_MUTATE  = 11;  // footswitch 2 (mutate)
constexpr uint8_t EXPR_PEDAL = 26;  // GP26 = ADC0
} // namespace phasebox::hal::pins
