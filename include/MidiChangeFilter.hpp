#pragma once
#include <cstddef>
#include <cstdint>

/// Decides whether a CC value needs sending: only when its channel, CC number or value changed, plus a
/// periodic refresh (so a synth plugged in later catches up). Keeps DIN MIDI (3125 bytes/s) uncongested.
class MidiChangeFilter {
public:
    static constexpr size_t MAX_SLOTS = 16;
    static constexpr uint32_t REFRESH_MS = 1000;

    bool should_send(size_t index, uint8_t channel, uint8_t cc, uint8_t value, uint32_t now_ms) {
        if (index >= MAX_SLOTS) return true;
        Slot& s = slots_[index];
        const bool send = !s.valid || s.channel != channel || s.cc != cc || s.value != value ||
                          static_cast<uint32_t>(now_ms - s.sent_ms) >= REFRESH_MS;
        if (send) s = Slot{true, channel, cc, value, now_ms};
        return send;
    }

private:
    struct Slot { bool valid; uint8_t channel, cc, value; uint32_t sent_ms; };
    Slot slots_[MAX_SLOTS] = {};
};
