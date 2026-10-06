#ifndef PHASEBOX_HAL_ENCODER_HPP
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
