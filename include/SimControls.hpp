#pragma once
// Page logic for the Phase Box UI, free of any graphics library so it is unit-tested on the host
// and shared by the native simulator (src/native_sim_main.cpp).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "PhaseBoxState.hpp"

namespace simui {

enum Page {
    P_WAVE = 0, P_BPM, P_PHASE, P_CHAN, P_CC, P_PM, P_MUTE, P_MAC_SEL, P_MAC_WGT, P_MAC_DIV, P_TUR_MIX,
    PAGE_COUNT
};

inline const char* page_name(int page) {
    static const char* names[PAGE_COUNT] = {
        "P1: WAVE", "P2: BPM", "P3: PHASE", "P4: CHAN", "P5: CC", "P6: PM",
        "P7: MUTE", "P8: MAC SEL", "P9: MAC WGT", "P10: MAC DIV", "P11: TUR MIX"};
    return names[((page % PAGE_COUNT) + PAGE_COUNT) % PAGE_COUNT];
}

inline int next_page(int page, int dir) { return ((page + dir) % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT; }

inline float round2(float v) { return std::round(v * 100.0f) / 100.0f; }

/// Apply one encoder step (dir = +1 / -1) to whatever the given page edits.
/// Pages 4, 5, 9, 10 edit the destination chosen on page 8 (`active_dest`).
inline void adjust(PhaseBoxState& s, int& active_dest, int page, int dir) {
    const int d = dir > 0 ? 1 : -1;
    const int last_dest = static_cast<int>(s.macros.destinations.size()) - 1;
    active_dest = std::clamp(active_dest, 0, std::max(0, last_dest));
    MacroDestination& dest = s.macros.destinations[active_dest];
    const float two_pi = 2.0f * static_cast<float>(M_PI);

    switch (page) {
        case P_WAVE:    s.osc.shape = static_cast<PhaseOscillator::WaveShape>(((int)s.osc.shape + d + 4) % 4); break;
        case P_BPM:     s.osc.bpm = std::clamp(s.osc.bpm + 5.0f * d, 40.0f, 300.0f); break;
        case P_PHASE: {
            float v = std::fmod(round2(s.osc.phase_offset + 0.2f * d), two_pi);
            s.osc.phase_offset = v < 0.0f ? v + two_pi : v;
            break;
        }
        case P_CHAN:    dest.midi_channel = std::clamp(dest.midi_channel + d, 1, 16); break;
        case P_CC:      dest.cc_number = std::clamp(dest.cc_number + d, 0, 127); break;
        case P_PM:      s.osc.pm_depth = std::clamp(round2(s.osc.pm_depth + 0.1f * d), 0.0f, 5.0f); break;
        case P_MUTE:    s.turing.mutation_prob = std::clamp(round2(s.turing.mutation_prob + 0.05f * d), 0.0f, 1.0f); break;
        case P_MAC_SEL: active_dest = std::clamp(active_dest + d, 0, std::max(0, last_dest)); break;
        case P_MAC_WGT: dest.scale = std::clamp(round2(dest.scale + 0.1f * d), -2.0f, 2.0f); break;
        case P_MAC_DIV: dest.clock_mult = MacroEngine::step_clock_mult(dest.clock_mult, d); break;
        case P_TUR_MIX: s.turing_mix = std::clamp(round2(s.turing_mix + 0.05f * d), 0.0f, 1.0f); break;
        default: break;
    }
}

/// Text for the bottom line of the OLED.
inline std::string footer(const PhaseBoxState& s, int page, int active_dest) {
    static const char* wave_names[] = {"SINE", "SAW", "SQR", "TRI"};
    const int idx = std::clamp(active_dest, 0, (int)s.macros.destinations.size() - 1);
    const MacroDestination& dest = s.macros.destinations[idx];
    char buf[64];
    switch (page) {
        case P_WAVE:    std::snprintf(buf, sizeof buf, "VAL: %s", wave_names[s.osc.shape & 3]); break;
        case P_BPM:     std::snprintf(buf, sizeof buf, "VAL: %d BPM", (int)s.osc.bpm); break;
        case P_PHASE:   std::snprintf(buf, sizeof buf, "VAL: %.2f", s.osc.phase_offset); break;
        case P_CHAN:    std::snprintf(buf, sizeof buf, "VAL: D%d CH %d", idx + 1, dest.midi_channel); break;
        case P_CC:      std::snprintf(buf, sizeof buf, "VAL: D%d CC %d", idx + 1, dest.cc_number); break;
        case P_PM:      std::snprintf(buf, sizeof buf, "VAL: PM %.2f", s.osc.pm_depth); break;
        case P_MUTE:    std::snprintf(buf, sizeof buf, "VAL: MUT %.2f", s.turing.mutation_prob); break;
        case P_MAC_SEL: std::snprintf(buf, sizeof buf, "VAL: DEST %d", idx + 1); break;
        case P_MAC_WGT: std::snprintf(buf, sizeof buf, "VAL: D%d WGT %.2f", idx + 1, dest.scale); break;
        case P_MAC_DIV: std::snprintf(buf, sizeof buf, "VAL: D%d x%g", idx + 1, dest.clock_mult); break;
        case P_TUR_MIX: std::snprintf(buf, sizeof buf, "VAL: MIX %.2f", s.turing_mix); break;
        default:        buf[0] = 0; break;
    }
    return buf;
}

/// How many destination cycles the preview window shows, so ratios are visible:
/// 8x shows 8 humps, 0.5x shows one full (two-beat) cycle.
inline int preview_cycles(float clock_mult) {
    return std::clamp(static_cast<int>(std::ceil(clock_mult - 1e-4f)), 1, 8);
}

/// The exact curve the active destination emits (shape, phase, PM, Turing mix, weight, offset,
/// expression all included), as values in [0, 1].
inline std::vector<float> preview_curve(const PhaseBoxState& s, int dest_index, int width) {
    const int idx = std::clamp(dest_index, 0, (int)s.macros.destinations.size() - 1);
    const MacroDestination& dest = s.macros.destinations[idx];
    MacroEngine::Source src;
    src.osc = &s.osc; src.turing_norm = s.turing_norm; src.turing_mix = s.turing_mix;
    const float span = 2.0f * static_cast<float>(M_PI) * preview_cycles(dest.clock_mult);
    std::vector<float> out(width > 0 ? width : 0);
    for (int x = 0; x < width; ++x) {
        out[x] = MacroEngine::sample(dest, span * static_cast<float>(x) / static_cast<float>(width),
                                     s.adc_expression, src);
    }
    return out;
}

/// X positions (pixels) of the live playhead(s) along the preview curve.
inline std::vector<int> preview_markers(const PhaseBoxState& s, int dest_index, int width) {
    const int idx = std::clamp(dest_index, 0, (int)s.macros.destinations.size() - 1);
    const int n = preview_cycles(s.macros.destinations[idx].clock_mult);
    const float frac = s.macros.dest_phase(idx) / (2.0f * static_cast<float>(M_PI));
    std::vector<int> xs;
    for (int k = 0; k < n; ++k) xs.push_back(static_cast<int>((frac + k) / n * width));
    return xs;
}

} // namespace simui
