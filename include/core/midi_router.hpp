// include/core/midi_router.hpp
#pragma once
#include <cstdint>
#include <array>

namespace phasebox::core {

struct MidiMessage {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
};

class MidiRouter {
public:
    MidiRouter() = default;

    void set_target_channel(uint8_t lfo_index, uint8_t channel) {
        if (lfo_index < 4 && channel >= 1 && channel <= 16) {
            channels_[lfo_index] = channel;
        }
    }

    void set_target_cc(uint8_t lfo_index, uint8_t cc_num) {
        if (lfo_index < 4 && cc_num <= 127) {
            cc_numbers_[lfo_index] = cc_num;
        }
    }

    MidiMessage generate_cc_message(uint8_t lfo_index, uint8_t value) const {
        if (lfo_index >= 4) return {0, 0, 0};
        
        uint8_t channel = channels_[lfo_index] - 1; // 0-indexed for MIDI status byte
        uint8_t status = 0xB0 | (channel & 0x0F);   // 0xB0 = Control Change
        uint8_t cc = cc_numbers_[lfo_index];
        uint8_t val = value & 0x7F;                 // MIDI 7-bit value (0-127)

        return {status, cc, val};
    }

private:
    std::array<uint8_t, 4> channels_{1, 2, 3, 4};
    std::array<uint8_t, 4> cc_numbers_{14, 15, 16, 17};
};

} // namespace phasebox::core