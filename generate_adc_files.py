#!/usr/bin/env python3
import os

FILES_TO_CREATE = {
    "include/hal/hal_adc.hpp": """#ifndef PHASEBOX_HAL_ADC_HPP
#define PHASEBOX_HAL_ADC_HPP

#include <cstdint>

namespace phasebox::hal {

class HalAdc {
public:
    HalAdc() = default;

    /**
     * @brief Initialize RP2040 ADC hardware (GPIO 26 / ADC 0).
     */
    void init();

    /**
     * @brief Read raw 12-bit ADC input (0 to 4095).
     * @return 12-bit unsigned integer.
     */
    uint16_t read_raw();

    /**
     * @brief Read EMA-smoothed normalized expression value.
     * @return Float in range [0.0f, 1.0f].
     */
    float read_normalized();

#ifdef HOST_BUILD
    /**
     * @brief Inject simulated raw ADC value for host unit tests.
     */
    void set_simulated_raw(uint16_t value) { simulated_raw_ = value; }
#endif

private:
    float smoothed_val_{0.0f};
    const float alpha_{0.15f}; // Smoothing factor (lower = smoother)

#ifdef HOST_BUILD
    uint16_t simulated_raw_{0};
#endif
};

} // namespace phasebox::hal

#endif // PHASEBOX_HAL_ADC_HPP
""",

    "src/hal/hal_adc.cpp": """#include "hal/hal_adc.hpp"

#ifndef HOST_BUILD
#include "pico/stdlib.h"
#include "hardware/adc.h"
#endif

namespace phasebox::hal {

void HalAdc::init() {
#ifndef HOST_BUILD
    adc_init();
    adc_gpio_init(26); // GPIO 26 is ADC Channel 0
    adc_select_input(0);
#endif
}

uint16_t HalAdc::read_raw() {
#ifndef HOST_BUILD
    adc_select_input(0);
    return adc_read();
#else
    return simulated_raw_;
#endif
}

float HalAdc::read_normalized() {
    uint16_t raw = read_raw();
    
    // Normalize raw 12-bit reading (0-4095) to [0.0f, 1.0f]
    float raw_norm = static_cast<float>(raw) / 4095.0f;
    if (raw_norm < 0.0f) raw_norm = 0.0f;
    if (raw_norm > 1.0f) raw_norm = 1.0f;

    // Apply Exponential Moving Average (EMA) filtering
    smoothed_val_ = (alpha_ * raw_norm) + ((1.0f - alpha_) * smoothed_val_);

    // Deadband clamping at extremes
    if (smoothed_val_ < 0.01f) return 0.0f;
    if (smoothed_val_ > 0.99f) return 1.0f;

    return smoothed_val_;
}

} // namespace phasebox::hal
""",

    "tests/test_hal_adc.cpp": """#include <gtest/gtest.h>
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
"""
}

def generate_files():
    for filepath, content in FILES_TO_CREATE.items():
        # Ensure directories exist
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        with open(filepath, "w") as f:
            f.write(content)
        print(f"Created/Updated: {filepath}")

if __name__ == "__main__":
    generate_files()

