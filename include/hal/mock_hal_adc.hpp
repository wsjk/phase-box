#pragma once
#include "hal/hal_adc.hpp"
#include <array>

namespace phasebox::hal {

class MockHalAdc : public HalAdc {
public:
    MockHalAdc() {
        channels_.fill(0.0f);
    }

    float read_normalized(uint8_t channel) override {
        if (channel < channels_.size()) {
            return channels_[channel];
        }
        return 0.0f;
    }

    void set_channel(uint8_t channel, float value) {
        if (channel < channels_.size()) {
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
            channels_[channel] = value;
        }
    }

private:
    std::array<float, 4> channels_{};
};

} // namespace phasebox::hal
