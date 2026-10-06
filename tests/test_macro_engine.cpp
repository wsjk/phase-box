#include "core/macro_engine.hpp"
#include <gtest/gtest.h>

using namespace phasebox::core;

TEST(MacroEngineTest, DefaultInitialization) {
  MacroEngine macro;
  EXPECT_FLOAT_EQ(macro.get_expression_value(), 0.0f);
  EXPECT_FLOAT_EQ(macro.get_weight(0), 0.0f);
  EXPECT_FLOAT_EQ(macro.get_weight(3), 0.0f);
  EXPECT_FLOAT_EQ(macro.get_weight(4), 0.0f); // Out of bounds returns 0.0f
}

TEST(MacroEngineTest, ExpressionValueClamping) {
  MacroEngine macro;

  // Test lower clamping
  macro.set_expression_value(-0.5f);
  EXPECT_FLOAT_EQ(macro.get_expression_value(), 0.0f);

  // Test upper clamping
  macro.set_expression_value(1.5f);
  EXPECT_FLOAT_EQ(macro.get_expression_value(), 1.0f);

  // Test valid mid-range
  macro.set_expression_value(0.75f);
  EXPECT_FLOAT_EQ(macro.get_expression_value(), 0.75f);
}

TEST(MacroEngineTest, WeightClampingAndBounds) {
  MacroEngine macro;

  // Test positive weight clamping
  macro.set_weight(0, 2.0f);
  EXPECT_FLOAT_EQ(macro.get_weight(0), 1.0f);

  // Test negative weight clamping
  macro.set_weight(1, -1.5f);
  EXPECT_FLOAT_EQ(macro.get_weight(1), -1.0f);

  // Ignore invalid LFO index (> 3)
  macro.set_weight(4, 1.0f);
  EXPECT_FLOAT_EQ(macro.get_weight(4), 0.0f);
}

TEST(MacroEngineTest, ApplyZeroWeightPassThrough) {
  MacroEngine macro;
  macro.set_expression_value(1.0f);
  macro.set_weight(0, 0.0f); // Inactive weight

  // Should return unchanged base_value
  EXPECT_EQ(macro.apply(0, 64), 64);
}

TEST(MacroEngineTest, ApplyBipolarModulation) {
  MacroEngine macro;
  macro.set_weight(0, 1.0f);

  // At 50% expression (0.5), offset is 0
  macro.set_expression_value(0.5f);
  EXPECT_EQ(macro.apply(0, 64), 64);

  // At 100% expression (1.0), offset is +63.5 -> 64 + 63.5 = 127.5 -> clamped
  // to 127
  macro.set_expression_value(1.0f);
  EXPECT_EQ(macro.apply(0, 64), 127);

  // At 0% expression (0.0), offset is -63.5 -> 64 - 63.5 = 0.5 -> 0
  macro.set_expression_value(0.0f);
  EXPECT_EQ(macro.apply(0, 64), 0);
}

TEST(MacroEngineTest, ApplyClamping) {
  MacroEngine macro;
  macro.set_weight(0, 1.0f);

  // Upper clamp test
  macro.set_expression_value(1.0f);
  EXPECT_EQ(macro.apply(0, 100), 127);

  // Lower clamp test
  macro.set_expression_value(0.0f);
  EXPECT_EQ(macro.apply(0, 20), 0);
}

TEST(MacroEngineTest, ApplyInvalidChannelIndex) {
  MacroEngine macro;
  // Out of bounds channel returns unmodified base value
  EXPECT_EQ(macro.apply(4, 99), 99);
}
