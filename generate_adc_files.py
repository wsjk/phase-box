#!/usr/bin/env python3
import os

# Dictionary mapping relative file paths to their C++ source code
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
"""
}

def generate_files():
    for filepath, content in FILES_TO_CREATE.items():
        # Automatically create missing parent directories (e.g. include/hal)
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        
        with open(filepath, "w") as f:
            f.write(content)
        print(f"Successfully created: {filepath}")

if __name__ == "__main__":
    generate_files()
