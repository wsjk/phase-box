#!/usr/bin/env python3
import os

FILES_TO_CREATE = {
    "include/hal/hal_encoder.hpp": """#ifndef PHASEBOX_HAL_ENCODER_HPP
#define PHASEBOX_HAL_ENCODER_HPP

#include <cstdint>

namespace phasebox::hal {

class HalEncoder {
public:
    HalEncoder() = default;

    /**
     * @brief Initialize GPIO pins for rotary encoder (Phase A, Phase B, Switch).
     */
    void init(uint8_t pin_a = 2, uint8_t pin_b = 3, uint8_t pin_sw = 4);

    /**
     * @brief Poll encoder hardware or update state.
     * @param current_time_us Current system time in microseconds.
     */
    void update(uint32_t current_time_us);

    /**
     * @brief Get and reset rotation delta since last call.
     * @return Positive for clockwise, negative for counter-clockwise.
     */
    int get_delta();

    /**
     * @brief Check if button is currently pressed.
     */
    bool is_button_pressed() const { return button_pressed_; }

    /**
     * @brief Get button press duration in milliseconds.
     */
    uint32_t get_button_hold_time_ms() const { return button_hold_time_ms_; }

#ifdef HOST_BUILD
    /**
     * @brief Inject simulated turns for host unit tests.
     */
    void simulate_turn(int delta) { delta_accum_ += delta; }

    /**
     * @brief Inject simulated button state for host unit tests.
     */
    void simulate_button(bool pressed, uint32_t hold_time_ms) {
        button_pressed_ = pressed;
        button_hold_time_ms_ = hold_time_ms;
    }
#endif

private:
    uint8_t pin_a_{2};
    uint8_t pin_b_{3};
    uint8_t pin_sw_{4};

    int delta_accum_{0};
    bool button_pressed_{false};
    uint32_t button_hold_time_ms_{0};

    uint8_t prev_state_{0};
    uint32_t last_turn_time_us_{0};
    uint32_t press_start_time_us_{0};
};

} // namespace phasebox::hal

#endif // PHASEBOX_HAL_ENCODER_HPP
""",

    "src/hal/hal_encoder.cpp": """#include "hal/hal_encoder.hpp"

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
""",

    "tests/test_hal_encoder.cpp": """#include <gtest/gtest.h>
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
"""
}

def generate_files():
    for filepath, content in FILES_TO_CREATE.items():
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        with open(filepath, "w") as f:
            f.write(content)
        print(f"Created/Updated: {filepath}")

if __name__ == "__main__":
    generate_files()

