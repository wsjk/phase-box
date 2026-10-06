// include/core/modulation_engine.hpp
#pragma once
#include "core/phase_lfo.hpp"
#include "core/clock_manager.hpp"
#include "core/midi_router.hpp"
#include "core/macro_engine.hpp"
#include <array>
#include <vector>
#include <algorithm>

namespace phasebox::core {

class ModulationEngine {
public:
    ModulationEngine() {
        for (size_t i = 0; i < lfos_.size(); ++i) {
            router_.set_target_channel(static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1));
            router_.set_target_cc(static_cast<uint8_t>(i), static_cast<uint8_t>(14 + i));
            for (size_t j = 0; j < lfos_.size(); ++j) {
                cross_mod_[i][j] = 0.0f;
            }
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

    MacroEngine& get_macro_engine() {
        return macro_engine_;
    }

    const MacroEngine& get_macro_engine() const {
        return macro_engine_;
    }

    /**
     * @brief Set cross-modulation depth between two LFOs.
     * @param source Source LFO index (0 to 3).
     * @param dest Destination LFO index (0 to 3).
     * @param depth Modulation depth (-1.0f to 1.0f).
     */
    void set_cross_modulation(size_t source, size_t dest, float depth) {
        if (source < 4 && dest < 4 && source != dest) {
            if (depth < -1.0f) depth = -1.0f;
            if (depth > 1.0f) depth = 1.0f;
            cross_mod_[source][dest] = depth;
        }
    }

    float get_cross_modulation(size_t source, size_t dest) const {
        if (source < 4 && dest < 4) {
            return cross_mod_[source][dest];
        }
        return 0.0f;
    }

    std::vector<MidiMessage> tick(uint32_t current_time_us) {
        clock_manager_.update(current_time_us);

        // 1. Sample raw LFO outputs
        std::array<uint8_t, 4> raw_vals;
        for (size_t i = 0; i < lfos_.size(); ++i) {
            raw_vals[i] = lfos_[i].update();
        }

        // 2. Apply nested cross-modulation
        std::array<uint8_t, 4> nested_vals = raw_vals;
        for (size_t dest = 0; dest < 4; ++dest) {
            float mod_offset = 0.0f;
            for (size_t src = 0; src < 4; ++src) {
                float depth = cross_mod_[src][dest];
                if (depth != 0.0f) {
                    mod_offset += (static_cast<float>(raw_vals[src]) - 64.0f) * depth;
                }
            }
            if (mod_offset != 0.0f) {
                float combined = static_cast<float>(raw_vals[dest]) + mod_offset;
                if (combined < 0.0f) combined = 0.0f;
                if (combined > 127.0f) combined = 127.0f;
                nested_vals[dest] = static_cast<uint8_t>(combined);
            }
        }

        // 3. Apply expression macro scaling and generate MIDI messages
        std::vector<MidiMessage> messages;
        messages.reserve(4);

        for (size_t i = 0; i < lfos_.size(); ++i) {
            uint8_t scaled_val = macro_engine_.apply(static_cast<uint8_t>(i), nested_vals[i]);
            messages.push_back(router_.generate_cc_message(static_cast<uint8_t>(i), scaled_val));
        }

        return messages;
    }

private:
    std::array<PhaseLFO, 4> lfos_;
    ClockManager clock_manager_;
    MidiRouter router_;
    MacroEngine macro_engine_;
    std::array<std::array<float, 4>, 4> cross_mod_{};
};

} // namespace phasebox::core