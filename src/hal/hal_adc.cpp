#include "hal/hal_adc.hpp"

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

    // Apply Exponential Moving Average (EMA) filtering first so step response works
    smoothed_val_ = (alpha_ * raw_norm) + ((1.0f - alpha_) * smoothed_val_);

    // Deadband clamping at extremes after smoothing
    if (smoothed_val_ < 0.01f) return 0.0f;
    if (smoothed_val_ > 0.95f) return 1.0f;

    return smoothed_val_;
}

} // namespace phasebox::hal
