#pragma once
#include <vector>
#include <algorithm>

struct MacroDestination {
    int cc_number = 16;     // Target MIDI CC
    int midi_channel = 1;   // MIDI Channel [1-16]
    float scale = 1.0f;     // Weight factor [-2.0 to 2.0]
    float offset = 0.0f;    // Baseline offset [-1.0 to 1.0]
};

class MacroEngine {
public:
    std::vector<MacroDestination> destinations;

    MacroEngine() {
        // Destination 1: Direct control on CC 16 (Full range, Ch 1)
        destinations.push_back({16, 1, 1.0f, 0.0f});

        // Destination 2: Inverted cross-modulation on CC 74 (Half depth, shifted up, Ch 1)
        destinations.push_back({74, 1, -0.5f, 0.5f});

        // Destination 3: Subtle background modulation on CC 10 (Narrow range, Ch 2)
        destinations.push_back({10, 2, 0.25f, 0.25f});
    }

    struct EvaluatedMIDI {
        int channel;
        int cc;
        int value;
    };

    std::vector<EvaluatedMIDI> evaluate(float lfo_value) {
        std::vector<EvaluatedMIDI> messages;
        for (const auto& dest : destinations) {
            float scaled = (lfo_value * dest.scale) + dest.offset;
            scaled = std::clamp(scaled, 0.0f, 1.0f);
            int midi_val = static_cast<int>(scaled * 127.0f);
            midi_val = std::clamp(midi_val, 0, 127);
            messages.push_back({dest.midi_channel, dest.cc_number, midi_val});
        }
        return messages;
    }
};
