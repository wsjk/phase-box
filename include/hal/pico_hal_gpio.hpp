#pragma once
#include "hal/hal_gpio.hpp"
#include <cstdint>

#if __has_include("hardware/gpio.h")
#include "hardware/gpio.h"
#define PHASEBOX_PICO_GPIO_AVAILABLE 1
#endif

namespace phasebox::hal {

class PicoHalGpio : public HalGpio {
public:
    // Default GPIO pin mapping on RP2040
    static constexpr uint PIN_ENC_A = 2;
    static constexpr uint PIN_ENC_B = 3;
    static constexpr uint PIN_ENC_SW = 6;
    static constexpr uint PIN_FS_TAP = 10;
    static constexpr uint PIN_FS_MUT = 11;

#if defined(PHASEBOX_PICO_GPIO_AVAILABLE)
    PicoHalGpio(uint pin_enc_a = PIN_ENC_A, uint pin_enc_b = PIN_ENC_B,
                uint pin_enc_sw = PIN_ENC_SW, uint pin_fs_tap = PIN_FS_TAP,
                uint pin_fs_mut = PIN_FS_MUT)
        : pin_enc_a_(pin_enc_a), pin_enc_b_(pin_enc_b), pin_enc_sw_(pin_enc_sw),
          pin_fs_tap_(pin_fs_tap), pin_fs_mut_(pin_fs_mut) {}

    void init() {
        // Configure tactile buttons & encoder switch with pull-ups (active low)
        uint pins[] = {pin_enc_a_, pin_enc_b_, pin_enc_sw_, pin_fs_tap_, pin_fs_mut_};
        for (uint p : pins) {
            gpio_init(p);
            gpio_set_dir(p, GPIO_IN);
            gpio_pull_up(p);
        }
        last_state_a_ = gpio_get(pin_enc_a_);
    }

    bool read_button(ButtonId id) override {
        switch (id) {
            case ButtonId::FootswitchTapMode:
                return !gpio_get(pin_fs_tap_);
            case ButtonId::FootswitchMutation:
                return !gpio_get(pin_fs_mut_);
            case ButtonId::EncoderSwitch:
                return !gpio_get(pin_enc_sw_);
        }
        return false;
    }

    int32_t read_encoder_delta() override {
        bool cur_a = gpio_get(pin_enc_a_);
        bool cur_b = gpio_get(pin_enc_b_);
        int32_t delta = 0;

        if (cur_a != last_state_a_) {
            // Rising or falling edge on channel A
            if (cur_a != cur_b) {
                delta = 1;
            } else {
                delta = -1;
            }
        }
        last_state_a_ = cur_a;
        return delta;
    }

private:
    uint pin_enc_a_;
    uint pin_enc_b_;
    uint pin_enc_sw_;
    uint pin_fs_tap_;
    uint pin_fs_mut_;
    bool last_state_a_{true};

#else
    PicoHalGpio() {}
    void init() {}
    bool read_button(ButtonId) override { return false; }
    int32_t read_encoder_delta() override { return 0; }
#endif
};

} // namespace phasebox::hal
