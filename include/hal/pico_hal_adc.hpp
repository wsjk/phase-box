#pragma once
#include "hal/hal_adc.hpp"
#include <cstdint>

#if __has_include("hardware/adc.h") && __has_include("hardware/gpio.h")
#include "hardware/adc.h"
#include "hardware/gpio.h"
#define PHASEBOX_PICO_ADC_AVAILABLE 1
#endif

namespace phasebox::hal {

class PicoHalAdc : public HalAdc {
public:
    static constexpr uint PIN_EXP_PEDAL = 26; // GP26 = ADC0

#if defined(PHASEBOX_PICO_ADC_AVAILABLE)
    PicoHalAdc(uint exp_pin = PIN_EXP_PEDAL) : exp_pin_(exp_pin) {}

    void init() {
        adc_init();
        adc_gpio_init(exp_pin_);
    }

    float read_normalized(uint8_t channel) override {
        if (channel > 3) return 0.0f;
        adc_select_input(channel);
        uint16_t raw = adc_read(); // 12-bit ADC (0 - 4095)
        return static_cast<float>(raw) / 4095.0f;
    }

private:
    uint exp_pin_;

#else
    PicoHalAdc(uint = 26) {}
    void init() {}
    float read_normalized(uint8_t) override { return 0.0f; }
#endif
};

} // namespace phasebox::hal
