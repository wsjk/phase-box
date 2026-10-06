#include <gtest/gtest.h>
#include "hal/hal_adc.hpp"

using namespace phasebox::hal;

TEST(HalAdcTest, SimulatedRawReadout) {
    HalAdc adc;
    adc.init();

    adc.set_simulated_raw(2048);
    EXPECT_EQ(adc.read_raw(), 2048);

    adc.set_simulated_raw(4095);
    EXPECT_EQ(adc.read_raw(), 4095);
}

TEST(HalAdcTest, EMASmoothingStepResponse) {
    HalAdc adc;
    adc.init();

    // Inject maximum ADC value (4095 -> 1.0f)
    adc.set_simulated_raw(4095);

    // Call 1: alpha = 0.15 -> 0.15 * 1.0 = 0.15
    float val1 = adc.read_normalized();
    EXPECT_NEAR(val1, 0.15f, 0.01f);

    // Call 2: 0.15 * 1.0 + 0.85 * 0.15 = 0.2775
    float val2 = adc.read_normalized();
    EXPECT_NEAR(val2, 0.2775f, 0.01f);

    // Run 30 iterations: smoothed value should converge towards 1.0f
    for (int i = 0; i < 30; ++i) {
        adc.read_normalized();
    }
    EXPECT_FLOAT_EQ(adc.read_normalized(), 1.0f);
}

TEST(HalAdcTest, LowerDeadbandClamping) {
    HalAdc adc;
    adc.init();

    // Raw value close to 0 (noise floor)
    adc.set_simulated_raw(20);
    
    for (int i = 0; i < 20; ++i) {
        adc.read_normalized();
    }

    EXPECT_FLOAT_EQ(adc.read_normalized(), 0.0f);
}

TEST(HalAdcTest, UpperDeadbandClamping) {
    HalAdc adc;
    adc.init();

    adc.set_simulated_raw(4080);
    
    for (int i = 0; i < 20; ++i) {
        adc.read_normalized();
    }

    EXPECT_FLOAT_EQ(adc.read_normalized(), 1.0f);
}
