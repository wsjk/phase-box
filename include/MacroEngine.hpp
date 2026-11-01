#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include "PhaseOscillator.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct MacroDestination {
    int cc_number = 16;     // Target MIDI CC
    int midi_channel = 1;   // MIDI Channel [1-16]
    float scale = 1.0f;     // Weight factor [-2.0 to 2.0]
    float offset = 0.0f;    // Baseline offset [-1.0 to 1.0]
    float clock_mult = 1.0f; // Polyrhythmic clock multiplier/subdivision [0.125 to 8.0]
};

class MacroEngine {
public:
    std::vector<MacroDestination> destinations;

    MacroEngine() {
        // Destination 1: Direct control on CC 16 (Full range, Ch 1)
        destinations.push_back({16, 1, 1.0f, 0.0f, 1.0f});

        // Destination 2: Inverted cross-modulation on CC 74 (Half depth, shifted up, Ch 1)
        destinations.push_back({74, 1, -0.5f, 0.5f, 1.5f});

        // Destination 3: Subtle background modulation on CC 10 (Narrow range, Ch 2)
        destinations.push_back({10, 2, 0.25f, 0.25f, 0.5f});
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

    /// Zero the destination phase accumulators (call alongside a master phase reset / tap).
    void reset_phases() {
        dest_phases_.clear();
        have_last_master_ = false;
    }

    /// Everything that shapes a destination's waveform besides its own scale/offset/multiplier.
    /// A default-constructed Source gives a plain sine with no Turing blend (the original behaviour).
    struct Source {
        const PhaseOscillator* osc = nullptr; // shape (P1), phase offset (P3), phase modulation (P6)
        float turing_norm = 0.0f;             // latest Turing value, 0..1
        float turing_mix = 0.0f;              // 0 = pure waveform, 1 = pure Turing sequence (P11)
    };

    /// Output of one destination at its own phase, before conversion to 0..127. Result is clamped to [0, 1].
    static float sample(const MacroDestination& dest, float dest_phase, float expression, const Source& src) {
        float wave = src.osc ? src.osc->normalizedAt(dest_phase)
                             : (std::sin(dest_phase) * 0.5f) + 0.5f;
        float mix = std::clamp(src.turing_mix, 0.0f, 1.0f);
        float norm_lfo = (1.0f - mix) * wave + mix * std::clamp(src.turing_norm, 0.0f, 1.0f);
        float scaled = (norm_lfo * dest.scale * (0.5f + 0.5f * expression)) + dest.offset;
        return std::clamp(scaled, 0.0f, 1.0f);
    }

    /// Musical ladder for the clock multiplier (P10): 1/8x .. 8x.
    static float step_clock_mult(float current, int dir) {
        static constexpr float ladder[] = {0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 8.0f};
        constexpr int n = static_cast<int>(sizeof(ladder) / sizeof(ladder[0]));
        constexpr float eps = 1e-4f;
        if (dir > 0) {
            for (int i = 0; i < n; ++i) if (ladder[i] > current + eps) return ladder[i];
            return ladder[n - 1];
        }
        for (int i = n - 1; i >= 0; --i) if (ladder[i] < current - eps) return ladder[i];
        return ladder[0];
    }

    /// Current phase of destination i (0 .. 2*pi), for UI playheads.
    float dest_phase(size_t i) const { return i < dest_phases_.size() ? dest_phases_[i] : 0.0f; }

    std::vector<EvaluatedMIDI> evaluate(float master_phase, float dt, float expression) {
        return evaluate(master_phase, dt, expression, Source{});
    }

    std::vector<EvaluatedMIDI> evaluate(float master_phase, float dt, float expression, const Source& src) {
        (void)dt;
        const float two_pi = 2.0f * static_cast<float>(M_PI);

        // Each destination integrates the master's forward motion scaled by its clock_mult,
        // so 0.5x really takes two master cycles and 0.75x has no jump at the master wrap.
        if (dest_phases_.size() != destinations.size()) {
            dest_phases_.assign(destinations.size(), 0.0f);
            have_last_master_ = false;
        }
        float delta = 0.0f;
        if (have_last_master_) {
            delta = std::fmod(master_phase - last_master_phase_, two_pi);
            if (delta < 0.0f) delta += two_pi;
        }
        for (size_t i = 0; i < destinations.size(); ++i) {
            float p = have_last_master_
                ? dest_phases_[i] + delta * destinations[i].clock_mult
                : master_phase * destinations[i].clock_mult;
            p = std::fmod(p, two_pi);
            if (p < 0.0f) p += two_pi;
            dest_phases_[i] = p;
        }
        last_master_phase_ = master_phase;
        have_last_master_ = true;

        std::vector<EvaluatedMIDI> messages;
        for (size_t i = 0; i < destinations.size(); ++i) {
            const auto& dest = destinations[i];
            float dest_phase = dest_phases_[i];

            float scaled = sample(dest, dest_phase, expression, src);
            int midi_val = static_cast<int>(scaled * 127.0f);
            midi_val = std::clamp(midi_val, 0, 127);
            messages.push_back({dest.midi_channel, dest.cc_number, midi_val});
        }
        return messages;
    }

private:
    std::vector<float> dest_phases_;
    float last_master_phase_ = 0.0f;
    bool have_last_master_ = false;
};
