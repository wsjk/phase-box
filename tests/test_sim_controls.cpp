// The editable pages (P1..P11) must change what actually goes out as MIDI, not just the screen.
#include <gtest/gtest.h>
#include <cmath>
#include <set>
#include <string>
#include <vector>
#include "SimControls.hpp"
#include "PresetManager.hpp"

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

TEST(SimPages, BpmStepsByOneWhenTurnedSlowlyAndByFiveWhenSpun) {
    PhaseBoxState s; int d = 0;
    adjust(s, d, P_BPM, +1, /*fast=*/false);
    EXPECT_NEAR(s.osc.bpm, 121.0f, 0.001f);
    adjust(s, d, P_BPM, +1, /*fast=*/true);
    EXPECT_NEAR(s.osc.bpm, 126.0f, 0.001f);
    adjust(s, d, P_BPM, -1);                            // default stays 5 so the simulator keys are unchanged
    EXPECT_NEAR(s.osc.bpm, 121.0f, 0.001f);
    for (int i = 0; i < 400; ++i) adjust(s, d, P_BPM, +1, false);
    EXPECT_NEAR(s.osc.bpm, 300.0f, 0.001f);             // clamps at 300
}

TEST(SimPages, RestartPhasePutsEveryLfoBackAtZero) {
    PhaseBoxState s; s.adc_expression = 1.0f;
    for (int i = 0; i < 80; ++i) { s.advance(0.01f); s.evaluateMacros(0.01f); }   // 0.8 s = 1.6 cycles at 120 BPM
    EXPECT_TRUE(s.osc.phase > 1.0f);
    s.restart_phase();
    EXPECT_NEAR(s.osc.phase, 0.0f, 0.0001f);
    EXPECT_NEAR(s.macros.dest_phase(1), 0.0f, 0.0001f);
    int before = s.turing.step(); (void)before;
    s.advance(0.001f);                                  // restarting must not look like a cycle wrap
    EXPECT_TRUE(s.osc.phase > 0.0f);
}

// ---- reset everything to defaults ----
namespace {
std::vector<uint8_t> saved_bytes(const PhaseBoxState& s) {
    uint8_t buf[PresetManager::MAX_BYTES];
    size_t n = PresetManager::encode(s, buf, sizeof(buf));
    return std::vector<uint8_t>(buf, buf + n);
}
void scramble(PhaseBoxState& s) {
    s.osc.shape = PhaseOscillator::SQUARE; s.osc.bpm = 77.0f; s.osc.phase_offset = 2.0f; s.osc.pm_depth = 3.0f;
    s.osc.pm_ratio = 5.0f; s.osc.phase = 4.0f;
    s.turing.mutation_prob = 0.7f; s.turing_mix = 0.6f; s.turing_norm = 0.9f;
    s.midi_channel = 9; s.midi_cc_num = 99;
    s.macros.destinations[0] = {99, 9, -2.0f, 0.9f, 8.0f};
    s.macros.destinations[1].clock_mult = 0.125f; s.macros.destinations[2].midi_channel = 16;
    for (int i = 0; i < 5; ++i) s.step_turing();
    s.advance(0.1f); s.evaluateMacros(0.1f);
}
}

TEST(SimReset, EveryParameterGoesBackToItsDefault) {
    PhaseBoxState s, fresh;
    scramble(s);
    ASSERT_TRUE(saved_bytes(s) != saved_bytes(fresh));       // the scramble really changed things
    s.reset_parameters();
    EXPECT_TRUE(saved_bytes(s) == saved_bytes(fresh));       // identical to a freshly started unit, byte for byte
    EXPECT_NEAR(s.osc.phase, 0.0f, 0.0001f);
    EXPECT_NEAR(s.osc.pm_ratio, 2.0f, 0.0001f);              // the unsaved oscillator settings too
    EXPECT_NEAR(s.turing_norm, 0.5f, 0.0001f);
    EXPECT_NEAR(s.macros.dest_phase(0), 0.0f, 0.0001f);
    EXPECT_EQ(s.macros.destinations.size(), 3u);
    for (int i = 0; i < 8; ++i) EXPECT_EQ(s.turing.step(), fresh.turing.step());   // same Turing pattern as a new unit
}

TEST(SimReset, LeavesInputsAndTheUiPositionAlone) {
    PhaseBoxState s;
    s.adc_expression = 0.7f; s.active_page = 5; s.button_state = true;
    s.osc.bpm = 99.0f;
    s.reset_parameters();
    EXPECT_NEAR(s.adc_expression, 0.7f, 0.0001f);           // the pedal is an input, not a setting
    EXPECT_EQ(s.active_page, 5);
    EXPECT_TRUE(s.button_state);
}

TEST(SimReset, KeepsTheTuringRandomSeedChosenAtStartup) {
    PhaseBoxState a, b;
    a.turing.seed(42); b.turing.seed(42);
    a.turing.mutation_prob = 0.9f;
    a.reset_parameters();                                    // must not re-seed the generator
    a.turing.mutation_prob = 0.5f; b.turing.mutation_prob = 0.5f;
    for (int i = 0; i < 200; ++i) EXPECT_EQ(a.turing.step(), b.turing.step());
}

TEST(SimReset, TheMidiOutputIsTheDefaultOneAfterwards) {
    PhaseBoxState s, fresh;
    scramble(s);
    s.reset_parameters(); fresh.reset_parameters();
    auto a = s.evaluateMacros(0.001f), b = fresh.evaluateMacros(0.001f);
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) { EXPECT_EQ(a[i].channel, b[i].channel); EXPECT_EQ(a[i].cc, b[i].cc); EXPECT_EQ(a[i].value, b[i].value); }
    EXPECT_EQ(a[1].cc, 74);                                  // Destination 2 is back on CC 74
}

// ---- freeze and bypass ----
TEST(SimFreeze, HoldsEveryOutputWhateverHappensAndResumesFromThere) {
    PhaseBoxState s = fresh();
    run(s, 0, 130);                                           // somewhere mid-cycle
    s.toggle_freeze();
    const auto at_freeze = s.evaluateMacros(0.001f);
    const float phase = s.osc.phase;
    bool still = true;
    for (int i = 0; i < 500; ++i) {                           // time passes, and the pedal and settings change
        s.adc_expression = (i % 2) ? 0.0f : 1.0f;
        if (i == 100) s.osc.shape = PhaseOscillator::SQUARE;
        if (i == 200) s.macros.destinations[0].scale = 0.3f;
        s.advance(0.001f);
        const auto out = s.evaluateMacros(0.001f);
        for (size_t d = 0; d < out.size(); ++d) still = still && out[d].value == at_freeze[d].value;
    }
    EXPECT_TRUE(still);
    EXPECT_NEAR(s.osc.phase, phase, 1e-6f);                   // the clock stood still

    s.osc.shape = PhaseOscillator::SINE; s.macros.destinations[0].scale = 1.0f; s.adc_expression = 1.0f;
    s.toggle_freeze();                                        // let go: continues from where it stopped
    s.advance(0.001f);
    const int first = s.evaluateMacros(0.001f)[0].value;
    EXPECT_LE(std::abs(first - at_freeze[0].value), 2);       // no jump
    bool moved = false;
    for (int v : run(s, 0, 150)) moved = moved || std::abs(v - first) > 10;
    EXPECT_TRUE(moved);                                       // and it is moving again
}

TEST(SimFreeze, StopsTheTuringStepsToo) {
    PhaseBoxState s = fresh();
    s.set_frozen(true);
    const float norm = s.turing_norm;
    for (int i = 0; i < 2000; ++i) s.advance(0.001f);         // would be 4 master cycles
    EXPECT_NEAR(s.turing_norm, norm, 1e-6f);
    s.set_frozen(false);
    for (int i = 0; i < 600; ++i) s.advance(0.001f);
    EXPECT_TRUE(std::fabs(s.turing_norm - norm) > 0.001f);    // steps again after the master cycle completes
}

TEST(SimFreeze, FreezingBeforeAnythingWasEvaluatedHoldsTheFirstValues) {
    PhaseBoxState s = fresh();
    s.set_frozen(true);
    const auto a = s.evaluateMacros(0.001f);
    s.macros.destinations[1].scale = 0.1f; s.adc_expression = 0.0f;
    const auto b = s.evaluateMacros(0.001f);
    for (size_t d = 0; d < a.size(); ++d) EXPECT_EQ(a[d].value, b[d].value);
}

TEST(SimBypass, EverythingGoesToZeroWhileTheLfosKeepRunning) {
    PhaseBoxState s = fresh();
    run(s, 0, 100);
    s.bypassed = true;
    const float p0 = s.osc.phase;
    bool all_zero = true;
    for (int i = 0; i < 400; ++i) {
        s.advance(0.001f);
        for (const auto& m : s.evaluateMacros(0.001f)) all_zero = all_zero && m.value == 0;
        if (i == 150) s.adc_expression = 0.3f;                // the pedal makes no difference either
    }
    EXPECT_TRUE(all_zero);
    EXPECT_TRUE(std::fabs(s.osc.phase - p0) > 0.01f);         // the clock was running underneath
    s.adc_expression = 1.0f;
    s.bypassed = false;
    int sum = 0;
    for (int i = 0; i < 40; ++i) { s.advance(0.001f); for (const auto& m : s.evaluateMacros(0.001f)) sum += m.value; }
    EXPECT_GT(sum, 100);                                      // back to normal output
}

TEST(SimBypass, OverridesFreezeAndLettingGoOfBypassReturnsToTheHeldValues) {
    PhaseBoxState s = fresh();
    run(s, 0, 130);
    s.toggle_freeze();
    const auto held = s.evaluateMacros(0.001f);
    s.bypassed = true;
    for (const auto& m : s.evaluateMacros(0.001f)) EXPECT_EQ(m.value, 0);
    s.bypassed = false;
    const auto back = s.evaluateMacros(0.001f);
    for (size_t d = 0; d < held.size(); ++d) EXPECT_EQ(back[d].value, held[d].value);
}

TEST(SimBypass, PreviewShowsAFlatLineAndTheTagsAreRight) {
    PhaseBoxState s = fresh();
    EXPECT_TRUE(mode_tag(s) == nullptr);
    s.frozen = true;
    EXPECT_TRUE(std::string(mode_tag(s)) == "FROZEN");
    s.bypassed = true;
    EXPECT_TRUE(std::string(mode_tag(s)) == "BYPASS");        // bypass wins when both are on
    const auto curve = preview_curve(s, 0, 100);
    bool flat = true; for (float v : curve) flat = flat && v == 0.0f;
    EXPECT_TRUE(flat);
    EXPECT_EQ(preview_markers(s, 0, 100).size(), 0u);
}

TEST(SimFreeze, ResetDoesNotTouchFreezeOrBypass) {
    PhaseBoxState s;
    s.frozen = true; s.bypassed = true;
    s.reset_parameters();
    EXPECT_TRUE(s.frozen);
    EXPECT_TRUE(s.bypassed);
}
