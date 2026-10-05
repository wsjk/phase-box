// include/core/modulation_engine.hpp
#pragma once
#include "core/phase_lfo.hpp"
#include "core/clock_manager.hpp"
#include "core/midi_router.hpp"
#include <array>
#include <vector>

namespace phasebox::core {

class ModulationEngine {
public:
    ModulationEngine() {
        for (size_t i = 0; i < lfos_.size(); ++i) {
            router_.set_target_channel(static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1));
            router_.set_target_cc(static_cast<uint8_t>(i), static_cast<uint8_t>(14 + i));
        }
    }

    void set_bpm(float bpm) {
        clock_manager_.set_bpm(bpm);
        for (auto& lfo : lfos_) {
            lfo.set_frequency(bpm / 60.0f);
        }
    }

    PhaseLFO& get_lfo(size_t index) {
        return lfos_.at(index);
    }

    ClockManager& get_clock_manager() {
        return clock_manager_;
    }

    MidiRouter& get_router() {
        return router_;
    }

    std::vector<MidiMessage> tick(uint32_t current_time_us) {
        clock_manager_.update(current_time_us);
        std::vector<MidiMessage> messages;
        messages.reserve(4);

        for (size_t i = 0; i < lfos_.size(); ++i) {
            uint8_t val = lfos_[i].update();
            messages.push_back(router_.generate_cc_message(static_cast<uint8_t>(i), val));
        }

        return messages;
    }

private:
    std::array<PhaseLFO, 4> lfos_;
    ClockManager clock_manager_;
    MidiRouter router_;
};

} // namespace phasebox::core