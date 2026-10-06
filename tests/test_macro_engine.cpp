#include "core/macro_engine.hpp"
#include <gtest/gtest.h>

using namespace phasebox::core;

TEST(MacroEngineTest, DefaultInitialization) {
  MacroEngine macro;
  EXPECT_EQ(macro.get_destination_count(), 4);
  EXPECT_FLOAT_EQ(macro.get_input_value(), 0.0f);
}

TEST(MacroEngineTest, LinearScalingMinMax) {
  MacroEngine macro;

  // Configure Destination 0: map macro input [0.0, 1.0] to output range
  // [10.0, 50.0]
  macro.set_destination_bounds(0, 10.0f, 50.0f);

  // Test at 0% input
  macro.update_input(0.0f);
  EXPECT_FLOAT_EQ(macro.get_scaled_output(0), 10.0f);

  // Test at 50% input
  macro.update_input(0.5f);
  EXPECT_FLOAT_EQ(macro.get_scaled_output(0), 30.0f);

  // Test at 100% input
  macro.update_input(1.0f);
  EXPECT_FLOAT_EQ(macro.get_scaled_output(0), 50.0f);
}

TEST(MacroEngineTest, AdcRawInputMapping) {
  MacroEngine macro;
  macro.set_destination_bounds(0, 0.0f, 127.0f); // Map to 7-bit MIDI range

  // RP2040 12-bit ADC range is 0 to 4095
  macro.update_from_adc(2047); // ~50%
  EXPECT_NEAR(macro.get_scaled_output(0), 63.5f, 0.5f);

  macro.update_from_adc(4095); // 100%
  EXPECT_NEAR(macro.get_scaled_output(0), 127.0f, 0.1f);
}

TEST(MacroEngineTest, InvertedScaling) {
  MacroEngine macro;

  // Inverted range: 1.0 input yields min, 0.0 input yields max
  macro.set_destination_bounds(1, 100.0f, 0.0f);

  macro.update_input(0.0f);
  EXPECT_FLOAT_EQ(macro.get_scaled_output(1), 100.0f);

  macro.update_input(1.0f);
  EXPECT_FLOAT_EQ(macro.get_scaled_output(1), 0.0f);
}

TEST(MacroEngineTest, MuteDestination) {
  MacroEngine macro;
  macro.set_destination_bounds(0, 0.0f, 100.0f);
  macro.set_destination_enabled(0, false);

  macro.update_input(1.0f);
  // Disabled destination should return baseline default (0.0f)
  EXPECT_FLOAT_EQ(macro.get_scaled_output(0), 0.0f);
}
