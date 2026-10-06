#pragma once
#include "hal/hal_gpio.hpp"
#include <unordered_map>

namespace phasebox::hal {

class MockHalGpio : public HalGpio {
public:
    MockHalGpio() {
        buttons_[ButtonId::FootswitchTapMode] = false;
        buttons_[ButtonId::FootswitchMutation] = false;
        buttons_[ButtonId::EncoderSwitch] = false;
    }

    bool read_button(ButtonId id) override {
        auto it = buttons_.find(id);
        if (it != buttons_.end()) {
            return it->second;
        }
        return false;
    }

    int32_t read_encoder_delta() override {
        int32_t delta = encoder_delta_;
        encoder_delta_ = 0;
        return delta;
    }

    void set_button(ButtonId id, bool pressed) {
        buttons_[id] = pressed;
    }

    void set_encoder_delta(int32_t delta) {
        encoder_delta_ = delta;
    }

private:
    std::unordered_map<ButtonId, bool> buttons_;
    int32_t encoder_delta_{0};
};

} // namespace phasebox::hal
