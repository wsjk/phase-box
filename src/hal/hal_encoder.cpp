#include "hal/hal_encoder.hpp"

#ifndef HOST_BUILD
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#endif

namespace phasebox::hal {

void HalEncoder::init(uint8_t pin_a, uint8_t pin_b, uint8_t pin_sw) {
    pin_a_ = pin_a;
    pin_b_ = pin_b;
    pin_sw_ = pin_sw;

#ifndef HOST_BUILD
    gpio_init(pin_a_);
    gpio_set_dir(pin_a_, GPIO_IN);
    gpio_pull_up(pin_a_);

    gpio_init(pin_b_);
    gpio_set_dir(pin_b_, GPIO_IN);
    gpio_pull_up(pin_b_);

    gpio_init(pin_sw_);
    gpio_set_dir(pin_sw_, GPIO_IN);
    gpio_pull_up(pin_sw_);

    uint8_t a = gpio_get(pin_a_);
    uint8_t b = gpio_get(pin_b_);
    prev_state_ = (a << 1) | b;
#endif
}

void HalEncoder::update(uint32_t current_time_us) {
#ifndef HOST_BUILD
    // Read hardware pins
    uint8_t a = gpio_get(pin_a_);
    uint8_t b = gpio_get(pin_b_);
    uint8_t curr_state = (a << 1) | b;

    // Gray-code quadrature state transition check
    if (curr_state != prev_state_) {
        if ((prev_state_ == 0b00 && curr_state == 0b01) ||
            (prev_state_ == 0b01 && curr_state == 0b11) ||
            (prev_state_ == 0b11 && curr_state == 0b10) ||
            (prev_state_ == 0b10 && curr_state == 0b00)) {
            
            // Turn acceleration: fast turns (< 30ms interval) step by 2
            uint32_t interval = current_time_us - last_turn_time_us_;
            int step = (interval < 30000) ? 2 : 1;
            delta_accum_ += step;
            last_turn_time_us_ = current_time_us;

        } else if ((prev_state_ == 0b00 && curr_state == 0b10) ||
                   (prev_state_ == 0b10 && curr_state == 0b11) ||
                   (prev_state_ == 0b11 && curr_state == 0b01) ||
                   (prev_state_ == 0b01 && curr_state == 0b00)) {
            
            uint32_t interval = current_time_us - last_turn_time_us_;
            int step = (interval < 30000) ? 2 : 1;
            delta_accum_ -= step;
            last_turn_time_us_ = current_time_us;
        }
        prev_state_ = curr_state;
    }

    // Active-low button check (pull-up resistor enabled)
    bool raw_sw = !gpio_get(pin_sw_);
    if (raw_sw && !button_pressed_) {
        button_pressed_ = true;
        press_start_time_us_ = current_time_us;
        button_hold_time_ms_ = 0;
    } else if (raw_sw && button_pressed_) {
        button_hold_time_ms_ = (current_time_us - press_start_time_us_) / 1000;
    } else if (!raw_sw && button_pressed_) {
        button_pressed_ = false;
        button_hold_time_ms_ = 0;
    }
#endif
}

int HalEncoder::get_delta() {
    int ret = delta_accum_;
    delta_accum_ = 0;
    return ret;
}

} // namespace phasebox::hal
