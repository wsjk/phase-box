#pragma once
#include <cstdint>

namespace phasebox::hal {
class HalMidi {
public:
    virtual ~HalMidi() = default;
    virtual void send_cc(uint8_t channel, uint8_t controller, uint8_t value) = 0;
    virtual bool poll_clock_tick() = 0;
};
}
