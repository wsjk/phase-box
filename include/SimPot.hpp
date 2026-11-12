#pragma once
// A pot-style view of the current page's value, for the simulator's on-screen knob: every page has a range, the
// value maps to a position 0..1 (min stop to max stop) and back. The unit itself has an endless encoder; this only
// changes how the simulated knob feels. Works with any page list: page 11 (P12 OFFSET) is used when it exists.
#include <algorithm>
#include <cmath>
#include "SimControls.hpp"

namespace simpot {

constexpr float SWEEP_DEGREES = 270.0f;                 // the knob turns from -135 (min) to +135 (max)
constexpr float MIN_DEGREES = -SWEEP_DEGREES / 2.0f;
constexpr int OFFSET_PAGE = 11;                         // "P12: MAC OFF" in builds that have it

inline const float* clock_ladder(int& count) {          // the multipliers P10 steps through
    static const float ladder[] = {0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 8.0f};
    count = static_cast<int>(sizeof(ladder) / sizeof(ladder[0]));
    return ladder;
}

inline bool has_offset_page() { return simui::PAGE_COUNT > OFFSET_PAGE; }

/// Position of the current page's value: 0 = min stop, 1 = max stop.
inline float norm(const PhaseBoxState& s, int dest, int page) {
    const int last = static_cast<int>(s.macros.destinations.size()) - 1;
    const MacroDestination& d = s.macros.destinations[std::clamp(dest, 0, std::max(0, last))];
    const float two_pi = 2.0f * static_cast<float>(M_PI);
    float n = 0.0f;
    switch (page) {
        case simui::P_WAVE:    n = static_cast<float>(s.osc.shape) / 3.0f; break;
        case simui::P_BPM:     n = (s.osc.bpm - 40.0f) / 260.0f; break;
        case simui::P_PHASE:   n = s.osc.phase_offset / two_pi; break;
        case simui::P_CHAN:    n = (d.midi_channel - 1) / 15.0f; break;
        case simui::P_CC:      n = d.cc_number / 127.0f; break;
        case simui::P_PM:      n = s.osc.pm_depth / 5.0f; break;
        case simui::P_MUTE:    n = s.turing.mutation_prob; break;
        case simui::P_MAC_SEL: n = last > 0 ? static_cast<float>(dest) / last : 0.0f; break;
        case simui::P_MAC_WGT: n = (d.scale + 2.0f) / 4.0f; break;
        case simui::P_MAC_DIV: {
            int count; const float* ladder = clock_ladder(count);
            int best = 0;
            for (int i = 1; i < count; ++i) if (std::fabs(ladder[i] - d.clock_mult) < std::fabs(ladder[best] - d.clock_mult)) best = i;
            n = static_cast<float>(best) / (count - 1);
            break;
        }
        case simui::P_TUR_MIX: n = s.turing_mix; break;
        default:
            if (has_offset_page() && page == OFFSET_PAGE) n = (d.offset + 1.0f) / 2.0f;
            break;
    }
    return std::clamp(n, 0.0f, 1.0f);
}

/// Put the current page's value where the knob is (0..1), rounded to the page's natural steps.
inline void set_norm(PhaseBoxState& s, int& dest, int page, float p) {
    p = std::clamp(p, 0.0f, 1.0f);
    const int last = static_cast<int>(s.macros.destinations.size()) - 1;
    dest = std::clamp(dest, 0, std::max(0, last));
    MacroDestination& d = s.macros.destinations[dest];
    const float two_pi = 2.0f * static_cast<float>(M_PI);
    switch (page) {
        case simui::P_WAVE:    s.osc.shape = static_cast<PhaseOscillator::WaveShape>(static_cast<int>(std::lround(p * 3.0f))); break;
        case simui::P_BPM:     s.osc.bpm = std::round(40.0f + p * 260.0f); break;
        case simui::P_PHASE:   s.osc.phase_offset = std::min(simui::round2(p * two_pi), 6.28f); break;
        case simui::P_CHAN:    d.midi_channel = 1 + static_cast<int>(std::lround(p * 15.0f)); break;
        case simui::P_CC:      d.cc_number = static_cast<int>(std::lround(p * 127.0f)); break;
        case simui::P_PM:      s.osc.pm_depth = simui::round2(p * 5.0f); break;
        case simui::P_MUTE:    s.turing.mutation_prob = simui::round2(p); break;
        case simui::P_MAC_SEL: dest = last > 0 ? static_cast<int>(std::lround(p * last)) : 0; break;
        case simui::P_MAC_WGT: d.scale = simui::round2(-2.0f + p * 4.0f); break;
        case simui::P_MAC_DIV: { int count; const float* ladder = clock_ladder(count); d.clock_mult = ladder[static_cast<int>(std::lround(p * (count - 1)))]; break; }
        case simui::P_TUR_MIX: s.turing_mix = simui::round2(p); break;
        default:
            if (has_offset_page() && page == OFFSET_PAGE) d.offset = simui::round2(-1.0f + p * 2.0f);
            break;
    }
}

/// One click (keys, scroll wheel): the page's normal step, but a pot cannot wrap round: it stops at the ends.
inline void turn_clamped(PhaseBoxState& s, int& dest, int page, int dir, bool fast = true) {
    const PhaseBoxState before = s;
    const int dest_before = dest;
    const float n0 = norm(s, dest, page);
    simui::adjust(s, dest, page, dir, fast);
    const float n1 = norm(s, dest, page);
    if ((dir > 0 && n1 < n0 - 0.5f) || (dir < 0 && n1 > n0 + 0.5f)) { s = before; dest = dest_before; }   // it wrapped: undo
}

} // namespace simpot
