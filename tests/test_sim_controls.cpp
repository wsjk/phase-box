// The editable pages (P1..P11) must change what actually goes out as MIDI, not just the screen.
#include <gtest/gtest.h>
#include <cmath>
#include <set>
#include <string>
#include <vector>
#include "SimControls.hpp"

using namespace simui;

namespace {
// Run the shared tick for `steps` steps; return destination `dest`'s MIDI value each step.
std::vector<int> run(PhaseBoxState& s, int dest, int steps, float dt = 0.001f) {
    std::vector<int> v;
    for (int i = 0; i < steps; ++i) {
        s.advance(dt);
        v.push_back(s.evaluateMacros(dt)[dest].value);
    }
    return v;
}
int rising_crossings(const std::vector<int>& v, int threshold) {
    int n = 0;
    for (size_t i = 1; i < v.size(); ++i) if (v[i - 1] <= threshold && v[i] > threshold) ++n;
    return n;
}
PhaseBoxState fresh() { PhaseBoxState s; s.adc_expression = 1.0f; return s; } // 120 BPM: one master cycle = 0.5 s
} // namespace

TEST(SimPages, NavigationWrapsAcrossAllElevenPages) {
    EXPECT_EQ(PAGE_COUNT, 11);
    EXPECT_EQ(next_page(10, +1), 0);
    EXPECT_EQ(next_page(0, -1), 10);
    EXPECT_TRUE(std::string(page_name(9)) == "P10: MAC DIV");
    EXPECT_TRUE(std::string(page_name(10)) == "P11: TUR MIX");
}

TEST(SimPages, FootersShowTheEditedValues) {
    PhaseBoxState s; int d = 0;
    EXPECT_TRUE(footer(s, P_WAVE, d) == "VAL: SINE");
    EXPECT_TRUE(footer(s, P_BPM, d) == "VAL: 120 BPM");
    EXPECT_TRUE(footer(s, P_CHAN, d) == "VAL: D1 CH 1");
    EXPECT_TRUE(footer(s, P_CC, d) == "VAL: D1 CC 16");
    EXPECT_TRUE(footer(s, P_MAC_SEL, d) == "VAL: DEST 1");
    EXPECT_TRUE(footer(s, P_MAC_WGT, d) == "VAL: D1 WGT 1.00");
    EXPECT_TRUE(footer(s, P_MAC_DIV, d) == "VAL: D1 x1");
    EXPECT_TRUE(footer(s, P_TUR_MIX, d) == "VAL: MIX 0.00");
    adjust(s, d, P_MAC_DIV, +1);
    EXPECT_TRUE(footer(s, P_MAC_DIV, d) == "VAL: D1 x1.5");
}

TEST(SimPages, PhaseOffsetWrapsInsteadOfGrowingForever) {
    PhaseBoxState s; int d = 0;
    for (int i = 0; i < 40; ++i) adjust(s, d, P_PHASE, +1);
    EXPECT_GE(s.osc.phase_offset, 0.0f);
    EXPECT_LE(s.osc.phase_offset, 6.2832f);
    adjust(s, d, P_PHASE, -1);
    EXPECT_GE(s.osc.phase_offset, 0.0f);
}

// ---- P4 / P5: per-destination channel and CC ----
TEST(SimPages, ChannelAndCcEditTheActiveDestinationAndReachMidi) {
    PhaseBoxState s; int d = 0;
    adjust(s, d, P_MAC_SEL, +1);                       // select Dest 2 (defaults: ch 1, CC 74)
    EXPECT_EQ(d, 1);
    adjust(s, d, P_CHAN, +1);
    adjust(s, d, P_CC, -1);
    EXPECT_EQ(s.macros.destinations[1].midi_channel, 2);
    EXPECT_EQ(s.macros.destinations[1].cc_number, 73);
    EXPECT_EQ(s.macros.destinations[0].midi_channel, 1);   // others untouched
    EXPECT_EQ(s.macros.destinations[0].cc_number, 16);
    auto out = s.evaluateMacros(0.01f);
    EXPECT_EQ(out[1].channel, 2);                      // the MIDI message itself changed
    EXPECT_EQ(out[1].cc, 73);
}

TEST(SimPages, ChannelAndCcClampToMidiRanges) {
    PhaseBoxState s; int d = 0;
    for (int i = 0; i < 30; ++i) adjust(s, d, P_CHAN, +1);
    EXPECT_EQ(s.macros.destinations[0].midi_channel, 16);
    for (int i = 0; i < 30; ++i) adjust(s, d, P_CHAN, -1);
    EXPECT_EQ(s.macros.destinations[0].midi_channel, 1);
    for (int i = 0; i < 200; ++i) adjust(s, d, P_CC, +1);
    EXPECT_EQ(s.macros.destinations[0].cc_number, 127);
    for (int i = 0; i < 200; ++i) adjust(s, d, P_CC, -1);
    EXPECT_EQ(s.macros.destinations[0].cc_number, 0);
}

// ---- P10: clock multiplier ----
TEST(SimPages, ClockLadderStepsMusicallyAndClamps) {
    EXPECT_FLOAT_EQ(MacroEngine::step_clock_mult(1.0f, +1), 1.5f);
    EXPECT_FLOAT_EQ(MacroEngine::step_clock_mult(1.0f, -1), 0.75f);
    EXPECT_FLOAT_EQ(MacroEngine::step_clock_mult(8.0f, +1), 8.0f);
    EXPECT_FLOAT_EQ(MacroEngine::step_clock_mult(0.125f, -1), 0.125f);
    EXPECT_FLOAT_EQ(MacroEngine::step_clock_mult(1.3f, +1), 1.5f);   // off-ladder preset values still step sensibly
    EXPECT_FLOAT_EQ(MacroEngine::step_clock_mult(1.3f, -1), 1.0f);
}

TEST(SimPages, ClockMultiplierChangesHowManyCyclesTheMidiMakes) {
    auto cycles_over_four_beats = [](float mult) {
        PhaseBoxState s = fresh(); int d = 0;
        s.macros.destinations[0].clock_mult = 1.0f;
        while (s.macros.destinations[0].clock_mult < mult - 1e-3f) adjust(s, d, P_MAC_DIV, +1);
        while (s.macros.destinations[0].clock_mult > mult + 1e-3f) adjust(s, d, P_MAC_DIV, -1);
        return rising_crossings(run(s, 0, 2000), 63);     // 2000 ms = 4 master cycles
    };
    int c1 = cycles_over_four_beats(1.0f), c2 = cycles_over_four_beats(2.0f), ch = cycles_over_four_beats(0.5f);
    EXPECT_GE(c1, 3); EXPECT_LE(c1, 5);       // ~4
    EXPECT_GE(c2, 7); EXPECT_LE(c2, 9);       // ~8
    EXPECT_GE(ch, 1); EXPECT_LE(ch, 3);       // ~2
}

// ---- P1 / P3 / P6: waveform settings must reach MIDI too ----
TEST(SimPages, WaveShapeReachesMidi) {
    PhaseBoxState sine = fresh();
    PhaseBoxState sq = fresh(); int d = 0;
    adjust(sq, d, P_WAVE, +1); adjust(sq, d, P_WAVE, +1);          // SINE -> SAW -> SQUARE
    EXPECT_EQ(sq.osc.shape, PhaseOscillator::SQUARE);
    std::set<int> a, b;
    for (int v : run(sine, 0, 600)) a.insert(v);
    for (int v : run(sq, 0, 600)) b.insert(v);
    EXPECT_GE(a.size(), 20u);   // smooth sine
    EXPECT_LE(b.size(), 2u);    // square is just two levels
}

TEST(SimPages, PhaseOffsetReachesMidi) {
    PhaseBoxState a = fresh(), b = fresh();
    b.osc.phase_offset = 1.5707963f;                               // +90 degrees
    int va = a.evaluateMacros(0.001f)[0].value;
    int vb = b.evaluateMacros(0.001f)[0].value;
    EXPECT_EQ(va, 63);
    EXPECT_EQ(vb, 127);
}

TEST(SimPages, PhaseModulationReachesMidi) {
    PhaseBoxState a = fresh(), b = fresh(); int d = 0;
    for (int i = 0; i < 20; ++i) adjust(b, d, P_PM, +1);           // PM depth 2.0
    auto va = run(a, 0, 500), vb = run(b, 0, 500);
    long diff = 0;
    for (size_t i = 0; i < va.size(); ++i) diff += std::abs(va[i] - vb[i]);
    EXPECT_GE(diff, 500L);
}

// ---- P7 / P11: Turing output reaches MIDI ----
TEST(SimPages, TuringMixBlendsTheSequenceIntoMidi) {
    PhaseBoxState s = fresh();
    s.turing_norm = 0.8f;
    s.turing_mix = 1.0f;
    for (int dest = 0; dest < 1; ++dest) {                          // dest 0: weight 1, offset 0
        EXPECT_EQ(s.evaluateMacros(0.001f)[dest].value, static_cast<int>(0.8f * 127.0f));
    }
    s.turing_mix = 0.0f;                                            // mix 0 = untouched LFO
    EXPECT_EQ(s.evaluateMacros(0.001f)[0].value, 63);
}

TEST(SimPages, TuringStepsOncePerMasterCycleAndMixIsEditable) {
    PhaseBoxState s = fresh(); int d = 0;
    EXPECT_NEAR(s.turing_norm, 0.5f, 0.0001f);
    run(s, 0, 600);                                                 // > one 500 ms master cycle
    EXPECT_TRUE(std::fabs(s.turing_norm - 0.5f) > 0.001f);          // stepped at the wrap
    for (int i = 0; i < 40; ++i) adjust(s, d, P_TUR_MIX, +1);
    EXPECT_NEAR(s.turing_mix, 1.0f, 0.0001f);                       // clamps at 100%
    for (int i = 0; i < 40; ++i) adjust(s, d, P_TUR_MIX, -1);
    EXPECT_NEAR(s.turing_mix, 0.0f, 0.0001f);
}

TEST(SimPages, ManualMutateStepsTheTuringMachine) {
    PhaseBoxState s;
    float before = s.turing_norm;
    s.step_turing();
    EXPECT_TRUE(s.turing_norm != before);
}

// ---- what the OLED draws is what the destination emits ----
TEST(SimPreview, CurveShowsTheRightNumberOfHumps) {
    auto humps = [](float mult) {
        PhaseBoxState s = fresh();
        s.macros.destinations[0].clock_mult = mult;
        auto c = preview_curve(s, 0, 240);
        int n = 0;
        for (size_t i = 1; i + 1 < c.size(); ++i) if (c[i] > c[i-1] && c[i] >= c[i+1] && c[i] > 0.9f) ++n;
        return n;
    };
    EXPECT_EQ(humps(1.0f), 1);
    EXPECT_EQ(humps(4.0f), 4);
    EXPECT_EQ(humps(0.5f), 1);    // 0.5x: window widens to show its one full (two-beat) cycle
    EXPECT_EQ(preview_cycles(8.0f), 8);
    EXPECT_EQ(preview_cycles(0.125f), 1);
}

TEST(SimPreview, CurveFollowsTuringMixAndPlayheadMatchesCycles) {
    PhaseBoxState s = fresh();
    s.turing_norm = 0.3f; s.turing_mix = 1.0f;
    auto c = preview_curve(s, 0, 120);
    bool flat = true;
    for (float v : c) flat = flat && std::fabs(v - 0.3f) < 1e-4f;
    EXPECT_TRUE(flat);
    s.macros.destinations[0].clock_mult = 3.0f;
    EXPECT_EQ(preview_markers(s, 0, 120).size(), 3u);
}
