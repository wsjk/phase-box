// include/core/clock_manager.hpp
#pragma once
#include <cstdint>

namespace phasebox::core {

enum class ClockSource {
    InternalTap,
    ExternalMIDI
};

class ClockManager {
public:
    ClockManager() = default;

    void process_midi_clock_byte(uint32_t current_time_us) {
        midi_clock_count_++;
        last_midi_clock_us_ = current_time_us;
        is_external_clock_active_ = true;

        if (midi_clock_count_ >= 24) { // 24 PPQN = 1 Quarter Note
            if (last_quarter_note_us_ != 0) {
                uint32_t delta = current_time_us - last_quarter_note_us_;
                if (delta > 0) {
                    bpm_ = 60000000.0f / static_cast<float>(delta);
                }
            }
            last_quarter_note_us_ = current_time_us;
            midi_clock_count_ = 0;
        }
    }

    void handle_tap_tempo(uint32_t current_time_us) {
        if (last_tap_time_us_ != 0) {
            uint32_t delta = current_time_us - last_tap_time_us_;
            // Valid tap tempo window: 20 BPM (3,000,000 us) to 300 BPM (200,000 us)
            if (delta >= 200000 && delta <= 3000000) {
                bpm_ = 60000000.0f / static_cast<float>(delta);
            }
        }
        last_tap_time_us_ = current_time_us;
    }

    void update(uint32_t current_time_us) {
        // Fallback to InternalTap if external MIDI clock stops (timeout = 1 second)
        if (is_external_clock_active_ && (current_time_us - last_midi_clock_us_ > 1000000)) {
            is_external_clock_active_ = false;
        }
    }

    float get_bpm() const {
        return bpm_;
    }

    ClockSource get_clock_source() const {
        return is_external_clock_active_ ? ClockSource::ExternalMIDI : ClockSource::InternalTap;
    }

    void set_bpm(float bpm) {
        if (bpm > 0.0f) {
            bpm_ = bpm;
        }
    }

private:
    float bpm_{120.0f};
    uint32_t last_tap_time_us_{0};
    uint32_t last_midi_clock_us_{0};
    uint32_t last_quarter_note_us_{0};
    uint8_t midi_clock_count_{0};
    bool is_external_clock_active_{false};
};

} // namespace phasebox::core
