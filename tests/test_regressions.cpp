// Regression tests for the issues found in the code review.
#include <gtest/gtest.h>
#include <array>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include "core/modulation_engine.hpp"
#include "core/core0_task.hpp"
#include "ui/ui_controller.hpp"
#include "MacroEngine.hpp"      // legacy ::MacroEngine (global namespace)
#include "PresetManager.hpp"    // firmware ::PresetManager (global namespace)

using namespace phasebox;
using phasebox::core::ModulationEngine;
using phasebox::core::Waveform;

namespace {
int count_rising_edges(ModulationEngine& e, int ticks) {
    e.get_lfo(0).set_waveform(Waveform::Square);
    int edges = 0;
    int prev = e.tick(1000)[0].data2;
    for (int i = 1; i <= ticks; ++i) {
        int v = e.tick(1000u + static_cast<uint32_t>(i) * 1000u)[0].data2;
        if (prev == 0 && v == 127) ++edges;
        prev = v;
    }
    return edges;
}

struct FakeMidi : hal::HalMidi {
    std::vector<std::array<uint8_t, 3>> sent;
    void send_cc(uint8_t ch, uint8_t cc, uint8_t v) override { sent.push_back({ch, cc, v}); }
    bool poll_clock_tick() override { return false; }
};
} // namespace

TEST(RegressionLFO, TriangleStaysInMidiRange) {
    core::PhaseLFO lfo;
    lfo.set_waveform(Waveform::Triangle);
    lfo.set_frequency(1.0f);
    int mx = 0, mn = 255;
    for (int i = 0; i < 2000; ++i) { int v = lfo.update(); mx = std::max(mx, v); mn = std::min(mn, v); }
    EXPECT_LE(mx, 127);
    EXPECT_GE(mx, 125);
    EXPECT_LE(mn, 2);
}

TEST(RegressionEngine, DefaultEngineRunsAt120Bpm) {
    ModulationEngine e;
    int edges = count_rising_edges(e, 6000); // 6 s @ 2 Hz
    EXPECT_GE(edges, 11);
    EXPECT_LE(edges, 12);
}

TEST(RegressionEngine, TapTempoChangesLfoSpeed) {
    ModulationEngine e;
    e.set_bpm(120.0f);
    e.get_clock_manager().handle_tap_tempo(1000000);
    e.get_clock_manager().handle_tap_tempo(1600000); // 600 ms -> 100 BPM
    EXPECT_NEAR(e.get_bpm(), 100.0f, 0.01f);
    int edges = count_rising_edges(e, 6000);          // 6 s @ 1.667 Hz = 10 cycles
    EXPECT_GE(edges, 9);
    EXPECT_LE(edges, 10);
}

TEST(RegressionEngine, TickReturnsFixedArray) {
    ModulationEngine e;
    auto msgs = e.tick(1000);
    static_assert(std::tuple_size<decltype(msgs)>::value == 4, "tick() must return std::array<MidiMessage,4>");
    EXPECT_EQ(msgs.size(), 4u);
}

TEST(RegressionCore0, SendsOnlyOnChangeAndWithinMidiBandwidth) {
    ModulationEngine e;
    FakeMidi midi;
    core::Core0Task task(e, midi);
    for (int i = 1; i <= 2000; ++i) task.step(static_cast<uint32_t>(i) * 1000u); // 2 s of sines
    const size_t bytes = midi.sent.size() * 3;
    EXPECT_LE(bytes, 5000u); // 80% of the 6250 B a DIN port carries in 2 s
    EXPECT_GE(midi.sent.size(), 40u); // but it is still actually modulating
}

TEST(RegressionCore0, StaticValuesAreSentOnce) {
    ModulationEngine e;
    for (size_t i = 0; i < 4; ++i) e.get_lfo(i).set_frequency(0.0f);
    FakeMidi midi;
    core::Core0Task task(e, midi);
    for (int i = 1; i <= 500; ++i) task.step(static_cast<uint32_t>(i) * 1000u);
    EXPECT_EQ(midi.sent.size(), 4u);
}

TEST(RegressionMacro, HalfSpeedDestinationCompletesFullCycleOverTwoBeats) {
    ::MacroEngine m; // dest[2]: 0.5x, scale .25, offset .25 -> full sine spans CC 31..63
    int lo = 127, hi = 0;
    const float two_pi = 2.0f * static_cast<float>(M_PI);
    float phase = 0.0f;
    for (int i = 0; i < 1300; ++i) { // ~2 master cycles
        auto out = m.evaluate(phase, 0.0166f, 1.0f);
        lo = std::min(lo, out[2].value); hi = std::max(hi, out[2].value);
        phase = std::fmod(phase + 0.01f, two_pi);
    }
    EXPECT_LE(lo, 33);
    EXPECT_GE(hi, 62);
}

TEST(RegressionMacro, NoJumpAtMasterWrapForFractionalRatio) {
    ::MacroEngine m;
    m.destinations[0].clock_mult = 0.75f; // scale 1, offset 0
    const float two_pi = 2.0f * static_cast<float>(M_PI);
    float phase = 0.0f; int prev = -1, max_step = 0;
    for (int i = 0; i < 2000; ++i) { // ~3 master cycles
        int v = m.evaluate(phase, 0.0166f, 1.0f)[0].value;
        if (prev >= 0) max_step = std::max(max_step, std::abs(v - prev));
        prev = v;
        phase = std::fmod(phase + 0.01f, two_pi);
    }
    EXPECT_LE(max_step, 2);
}

TEST(RegressionUI, IdleEncoderPollingDoesNotDisturbTurnVelocity) {
    ModulationEngine e; e.set_bpm(120.0f);
    ui::UIController ui(e); ui.init();
    ui.handle_encoder_click();                         // DASHBOARD -> BPM_SETTING
    ui.handle_encoder_turn(1, 1000000);                // first turn: fine step 0.1
    ui.handle_bpm_encoder_input(0, 1033000);           // idle poll (delta 0)
    ui.handle_encoder_turn(1, 1040000);                // 40 ms after the real turn: still fine
    EXPECT_NEAR(e.get_bpm(), 120.2f, 0.001f);
    ui.handle_encoder_turn(1, 1050000);                // 10 ms later: fast, +1.0
    EXPECT_NEAR(e.get_bpm(), 121.2f, 0.001f);
}

TEST(RegressionUI, TapStateIsPerInstance) {
    ModulationEngine e1, e2; e1.set_bpm(120.0f); e2.set_bpm(120.0f);
    ui::UIController u1(e1), u2(e2); u1.init(); u2.init();
    u1.handle_tap_tempo(1000000);
    u2.handle_tap_tempo(1600000); // first tap on u2 must not pair with u1's tap
    EXPECT_NEAR(e2.get_bpm(), 120.0f, 0.001f);
}

class RegressionPreset : public ::testing::Test {
protected:
    void SetUp() override { ::PresetManager::path_prefix = "/tmp/"; }
    void TearDown() override { std::remove("/tmp/preset_7.bin"); }
};

TEST_F(RegressionPreset, RoundTrip) {
    PhaseBoxState a; a.osc.bpm = 143.5f; a.midi_channel = 5; a.midi_cc_num = 74;
    a.macros.destinations[1].clock_mult = 0.75f; a.turing_mix = 0.35f;
    ASSERT_TRUE(::PresetManager::savePreset(7, a));
    PhaseBoxState b;
    ASSERT_TRUE(::PresetManager::loadPreset(7, b));
    EXPECT_FLOAT_EQ(b.osc.bpm, 143.5f);
    EXPECT_EQ(b.midi_channel, 5);
    EXPECT_EQ(b.midi_cc_num, 74);
    EXPECT_FLOAT_EQ(b.macros.destinations[1].clock_mult, 0.75f);
    EXPECT_FLOAT_EQ(b.turing_mix, 0.35f);
}

TEST_F(RegressionPreset, CorruptTruncatedAndOutOfRangeFilesAreRejectedWithoutSideEffects) {
    PhaseBoxState a; a.osc.bpm = 99.0f;
    ASSERT_TRUE(::PresetManager::savePreset(7, a));

    FILE* f = fopen("/tmp/preset_7.bin", "rb"); uint8_t raw[512]; size_t n = fread(raw, 1, sizeof(raw), f); fclose(f);
    auto write_raw = [&](const uint8_t* d, size_t len) { FILE* o = fopen("/tmp/preset_7.bin", "wb"); fwrite(d, 1, len, o); fclose(o); };

    PhaseBoxState victim; victim.osc.bpm = 120.0f;
    uint8_t flipped[512]; std::memcpy(flipped, raw, n); flipped[12] ^= 0xFF;   // bit rot
    write_raw(flipped, n);          EXPECT_FALSE(::PresetManager::loadPreset(7, victim));
    write_raw(raw, n - 7);          EXPECT_FALSE(::PresetManager::loadPreset(7, victim)); // truncated
    write_raw(raw, 3);              EXPECT_FALSE(::PresetManager::loadPreset(7, victim)); // tiny
    EXPECT_FLOAT_EQ(victim.osc.bpm, 120.0f); // untouched
    EXPECT_FALSE(::PresetManager::loadPreset(8, victim)); // missing file
}
