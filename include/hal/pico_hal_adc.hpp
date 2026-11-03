#pragma once
#include "hal/hal_adc.hpp"
#include "hal/pinout.hpp"
#include <cstdint>

#if __has_include("hardware/adc.h") && __has_include("hardware/gpio.h")
#include "hardware/adc.h"
#include "hardware/gpio.h"
#define PHASEBOX_PICO_ADC_AVAILABLE 1
#endif

namespace phasebox::hal {

class PicoHalAdc : public AdcInput {
public:
    static constexpr unsigned PIN_EXP_PEDAL = pins::EXPR_PEDAL; // GP26 = ADC0

#if defined(PHASEBOX_PICO_ADC_AVAILABLE)
    PicoHalAdc(unsigned exp_pin = PIN_EXP_PEDAL) : exp_pin_(exp_pin) {}

    void init() {
        adc_init();
        adc_gpio_init(exp_pin_);
    }

    float read_normalized() override {
        adc_select_input(exp_pin_ - 26); // GP26..GP29 -> ADC0..ADC3
        uint16_t raw = adc_read();       // 12-bit ADC (0 - 4095)
        return static_cast<float>(raw) / 4095.0f;
    }

private:
    unsigned exp_pin_;

#else
    PicoHalAdc(unsigned = 26) {}
    void init() {}
    float read_normalized() override { return 0.0f; }
#endif
};

} // namespace phasebox::hal
