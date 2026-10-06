#pragma once
#include "hal/hal_midi.hpp"
#include <vector>
#include <queue>

namespace phasebox::hal {

struct MockCcMessage {
    uint8_t channel;
    uint8_t controller;
    uint8_t value;
};

class MockHalMidi : public HalMidi {
public:
    void send_cc(uint8_t channel, uint8_t controller, uint8_t value) override {
        sent_messages_.push_back({channel, controller, value});
    }

    bool poll_clock_tick() override {
        if (clock_ticks_available_ > 0) {
            clock_ticks_available_--;
            return true;
        }
        return false;
    }

    void inject_clock_tick() {
        clock_ticks_available_++;
    }

    void inject_clock_ticks(size_t count) {
        clock_ticks_available_ += count;
    }

    const std::vector<MockCcMessage>& get_sent_messages() const {
        return sent_messages_;
    }

    void clear() {
        sent_messages_.clear();
        clock_ticks_available_ = 0;
    }

    size_t get_sent_count() const {
        return sent_messages_.size();
    }

private:
    std::vector<MockCcMessage> sent_messages_;
    size_t clock_ticks_available_{0};
};

} // namespace phasebox::hal
