#pragma once
#include <cstdint>

// Single source of truth for the board wiring (matches the README and docs/USER_GUIDE.md).
namespace phasebox::hal::pins {
constexpr uint8_t MIDI_TX    = 0;   // uart0 TX, 31,250 baud MIDI out (DIN/TRS)
constexpr uint8_t MIDI_RX    = 1;   // uart0 RX (reserved; MIDI in is not used yet)
constexpr uint8_t ENC_A      = 2;   // rotary encoder phase A
constexpr uint8_t ENC_B      = 3;   // rotary encoder phase B
constexpr uint8_t OLED_SDA   = 4;   // I2C0 SDA
constexpr uint8_t OLED_SCL   = 5;   // I2C0 SCL
constexpr uint8_t ENC_SW     = 6;   // encoder push switch
constexpr uint8_t EXP_ENC_A  = 7;   // expression encoder phase A
constexpr uint8_t EXP_ENC_B  = 8;   // expression encoder phase B
constexpr uint8_t EXP_ENC_SW = 9;   // expression encoder push switch (freeze)
constexpr uint8_t FS_TAP     = 10;  // footswitch 1 (tap / phase reset)
constexpr uint8_t FS_MUTATE  = 11;  // footswitch 2 (mutate)
constexpr uint8_t FS_BYPASS  = 12;  // footswitch 3 (bypass)

// Encoder behaviour (firmware): if turning clockwise lowers the value, set ENC_REVERSED (page/value encoder)
// or EXP_ENC_REVERSED (expression encoder) = true. If one click moves two steps, or two clicks are needed for one step, change the transitions per click
// (4 for most encoders with a click at every position, 2 for some). Both encoders share that setting.
constexpr bool ENC_REVERSED = false;
constexpr bool EXP_ENC_REVERSED = false;
constexpr int ENC_TRANSITIONS_PER_DETENT = 4;
} // namespace phasebox::hal::pins
