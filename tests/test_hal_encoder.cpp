#include <gtest/gtest.h>
#include "hal/hal_encoder.hpp"

using namespace phasebox::hal;

TEST(HalEncoderTest, InitialState) {
    HalEncoder encoder;
    encoder.init(2, 3, 4);
    EXPECT_EQ(encoder.get_delta(), 0);
    EXPECT_FALSE(encoder.is_button_pressed());
    EXPECT_EQ(encoder.get_button_hold_time_ms(), 0);
}

TEST(HalEncoderTest, SimulatedTurns) {
    HalEncoder encoder;
    encoder.init(2, 3, 4);

    encoder.simulate_turn(1);
    EXPECT_EQ(encoder.get_delta(), 1);
    EXPECT_EQ(encoder.get_delta(), 0); // Resets after reading

    encoder.simulate_turn(-3);
    EXPECT_EQ(encoder.get_delta(), -3);
}

TEST(HalEncoderTest, SimulatedButtonHold) {
    HalEncoder encoder;
    encoder.init(2, 3, 4);

    encoder.simulate_button(true, 1200);
    EXPECT_TRUE(encoder.is_button_pressed());
    EXPECT_EQ(encoder.get_button_hold_time_ms(), 1200);

    encoder.simulate_button(false, 0);
    EXPECT_FALSE(encoder.is_button_pressed());
    EXPECT_EQ(encoder.get_button_hold_time_ms(), 0);
}
