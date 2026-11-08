// The whole device, driven with fake pins: turn/click/hold the encoder, step on the footswitches,
// move the pedal, and check what comes out as MIDI, settings, saved presets and OLED pixels.
#include <gtest/gtest.h>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "DeviceApp.hpp"
#include "hal/mock_hal_display.hpp"

namespace {
struct Recorder : MidiOut {
    struct Msg { uint8_t ch, cc, val; uint32_t at_ms; };
    std::vector<Msg> sent;
    uint32_t* clock = nullptr;
    void cc(uint8_t ch, uint8_t c, uint8_t v) override { sent.push_back({ch, c, v, clock ? *clock : 0}); }
};
struct MemStorage : PresetStorage {
    std::map<int, std::vector<uint8_t>> slots; int writes = 0;
    bool write(int slot, const uint8_t* d, size_t n) override { ++writes; slots[slot].assign(d, d + n); return true; }
    size_t read(int slot, uint8_t* out, size_t cap) override {
        auto it = slots.find(slot);
        if (it == slots.end() || it->second.size() > cap) return 0;
        std::memcpy(out, it->second.data(), it->second.size());
        return it->second.size();
    }
};

// A rig that advances time 1 ms per step, polling the controls every step and running the 60 Hz frame on schedule.
struct Rig {
    PhaseBoxState state;
    MemStorage mem;
    NoStorage none;
    PresetSession presets;
    Recorder midi;
    DeviceApp app;
    DeviceInputs in;
    uint64_t now_us = 1000000;      // start at 1 s, as if booted a moment ago
    uint64_t next_frame_us = 1000000;
    uint32_t ms_clock = 1000;

    explicit Rig(bool with_storage = true, uint16_t oled_h = 32)
        : presets(with_storage ? static_cast<PresetStorage&>(mem) : static_cast<PresetStorage&>(none)),
          app(state, presets, midi, 128, oled_h) { midi.clock = &ms_clock; }

    void step() {
        now_us += 1000; ms_clock = static_cast<uint32_t>(now_us / 1000);
        app.poll_inputs(in, now_us);
        if (now_us >= next_frame_us) { app.frame(now_us); next_frame_us += DeviceApp::FRAME_US; }
    }
    void run_ms(int ms) { for (int i = 0; i < ms; ++i) step(); }

    void detent(int dir) {   // one click of the encoder, 1 ms per signal change
        static const bool cw[4][2]  = {{0,1},{0,0},{1,0},{1,1}};
        static const bool ccw[4][2] = {{1,0},{0,0},{0,1},{1,1}};
        for (int i = 0; i < 4; ++i) { in.enc_a = dir > 0 ? cw[i][0] : ccw[i][0]; in.enc_b = dir > 0 ? cw[i][1] : ccw[i][1]; step(); }
    }
    void press(bool DeviceInputs::*pin, int hold_ms) { in.*pin = true; run_ms(hold_ms); in.*pin = false; run_ms(40); }
};
} // namespace

TEST(Device, TurningTheEncoderEditsTheCurrentPage) {
    Rig r;
    r.run_ms(50);
    EXPECT_EQ(r.state.active_page, 0);
    r.detent(+1);
    EXPECT_EQ(r.state.osc.shape, PhaseOscillator::SAW);
    r.detent(-1); r.detent(-1);
    EXPECT_EQ(r.state.osc.shape, PhaseOscillator::TRIANGLE);   // wraps around
}

TEST(Device, ClickGoesToTheNextPageAndWrapsAndSavesNothing) {
    Rig r;
    r.run_ms(50);
    r.press(&DeviceInputs::enc_sw, 100);
    EXPECT_EQ(r.state.active_page, 1);
    for (int i = 0; i < 11; ++i) r.press(&DeviceInputs::enc_sw, 100);
    EXPECT_EQ(r.state.active_page, 0);                          // 12 pages, back to the start
    EXPECT_EQ(r.mem.writes, 0);
}

TEST(Device, HoldingForOneSecondSavesAndDoesNotAlsoChangePage) {
    Rig r;
    r.run_ms(50);
    r.detent(+1);                                               // an edit to save
    r.in.enc_sw = true;
    r.run_ms(900);
    EXPECT_EQ(r.mem.writes, 0);
    r.run_ms(200);
    EXPECT_GE(r.mem.writes, 1);
    r.in.enc_sw = false; r.run_ms(60);
    EXPECT_EQ(r.state.active_page, 0);                          // releasing after a hold is not a click

    PhaseBoxState back; MemStorage& m = r.mem;
    ASSERT_TRUE(PresetManager::decode(m.slots[1].data(), m.slots[1].size(), back));
    EXPECT_EQ(back.osc.shape, PhaseOscillator::SAW);
}

TEST(Device, SwitchBounceIsNotAClick) {
    Rig r;
    r.run_ms(50);
    for (int i = 0; i < 6; ++i) { r.in.enc_sw = true; r.run_ms(4); r.in.enc_sw = false; r.run_ms(4); }
    r.run_ms(100);
    EXPECT_EQ(r.state.active_page, 0);
}

TEST(Device, SlowTurnsAreFineAndFastSpinsAreCoarseOnTheBpmPage) {
    Rig r;
    r.run_ms(50);
    r.press(&DeviceInputs::enc_sw, 100);                        // page 2 = BPM
    ASSERT_EQ(r.state.active_page, 1);
    const float start = r.state.osc.bpm;
    for (int i = 0; i < 3; ++i) { r.detent(+1); r.run_ms(150); }
    EXPECT_NEAR(r.state.osc.bpm, start + 3.0f, 0.01f);          // 1 BPM per slow click
    r.run_ms(300);
    const float mid = r.state.osc.bpm;
    for (int i = 0; i < 3; ++i) r.detent(+1);                   // 4 ms apart: a fast spin
    EXPECT_NEAR(r.state.osc.bpm, mid + 1.0f + 5.0f + 5.0f, 0.01f);   // first click is still slow, then 5 per click
    r.run_ms(300);
    r.detent(-1);
    EXPECT_NEAR(r.state.osc.bpm, mid + 11.0f - 1.0f, 0.01f);
}

TEST(Device, TapFootswitchSetsTempoAndRestartsThePhase) {
    Rig r;
    r.run_ms(50);
    r.press(&DeviceInputs::tap, 30);                            // taps 600 ms apart = 100 BPM
    r.run_ms(530);
    r.press(&DeviceInputs::tap, 30);
    r.run_ms(530);
    r.state.osc.phase = 2.5f;                                   // somewhere mid-beat
    r.in.tap = true; r.run_ms(25);
    EXPECT_NEAR(r.state.osc.bpm, 100.0f, 2.0f);
    EXPECT_NEAR(r.state.osc.phase, 0.0f, 0.05f);                // restarted at 0 degrees by the tap
    r.in.tap = false; r.run_ms(50);
}

TEST(Device, TapBounceDoesNotDoubleCount) {
    Rig r;
    r.run_ms(50);
    const float before = r.state.osc.bpm;
    for (int i = 0; i < 5; ++i) { r.in.tap = true; r.run_ms(3); r.in.tap = false; r.run_ms(3); }   // glitches only
    r.run_ms(100);
    EXPECT_NEAR(r.state.osc.bpm, before, 0.001f);
}

TEST(Device, MutateFootswitchStepsTheTuringMachine) {
    Rig r;
    r.run_ms(50);
    const float before = r.state.turing_norm;
    r.press(&DeviceInputs::mutate, 40);
    EXPECT_TRUE(r.state.turing_norm != before);
    const float after_one = r.state.turing_norm;
    r.press(&DeviceInputs::mutate, 40);
    EXPECT_TRUE(r.state.turing_norm != after_one);
}

TEST(Device, ExpressionPedalScalesTheOutputButIsNotScreenActivity) {
    Rig r;
    r.run_ms(50);
    const uint32_t before = r.app.last_activity_ms();
    r.in.expression = 0.75f;
    r.run_ms(100);
    EXPECT_NEAR(r.state.adc_expression, 0.75f, 0.0001f);
    EXPECT_EQ(r.app.last_activity_ms(), before);
    r.in.expression = 3.0f; r.run_ms(10);
    EXPECT_NEAR(r.state.adc_expression, 1.0f, 0.0001f);          // out-of-range readings are clamped
}

TEST(Device, EncoderAndFootswitchesCountAsActivityForTheScreensaver) {
    Rig r;
    r.run_ms(100);
    uint32_t t = r.app.last_activity_ms();
    r.run_ms(100); r.detent(+1);
    EXPECT_GT(r.app.last_activity_ms(), t); t = r.app.last_activity_ms();
    r.run_ms(100); r.press(&DeviceInputs::tap, 30);
    EXPECT_GT(r.app.last_activity_ms(), t); t = r.app.last_activity_ms();
    r.run_ms(100); r.press(&DeviceInputs::mutate, 30);
    EXPECT_GT(r.app.last_activity_ms(), t);
}

TEST(Device, MidiIsSentOnChangeWithAPeriodicRefreshNotEveryFrame) {
    Rig r;
    for (auto& d : r.state.macros.destinations) d.scale = 0.0f;  // constant outputs
    r.run_ms(50);
    EXPECT_EQ(r.midi.sent.size(), 3u);                          // first frame announces all three destinations
    r.run_ms(900);
    EXPECT_EQ(r.midi.sent.size(), 3u);                          // 54 frames later: still nothing new
    r.run_ms(250);
    EXPECT_EQ(r.midi.sent.size(), 6u);                          // ~1 s refresh
    r.state.macros.destinations[0].cc_number = 99;
    r.run_ms(40);
    ASSERT_EQ(r.midi.sent.size(), 7u);                          // a changed CC number goes out at once
    EXPECT_EQ(r.midi.sent.back().cc, 99);
}

TEST(Device, ARunningLfoStaysWithinTheDinMidiByteBudget) {
    Rig r;
    r.state.adc_expression = 1.0f;
    r.run_ms(4000);
    const double bytes_per_sec = r.midi.sent.size() * 3.0 / 4.0;
    EXPECT_LT(bytes_per_sec, 3125.0 * 0.5);                     // under half of what a DIN port can carry
    EXPECT_GE(r.midi.sent.size(), 20u);                         // and it is clearly moving
}

TEST(Device, OledFramesRunAtAboutThirtyFps) {
    Rig r;
    int frames = 0;
    uint64_t t = 2000000;
    for (int i = 0; i < 120; ++i) { t += DeviceApp::FRAME_US; frames += r.app.frame(t) ? 1 : 0; }   // 2 s of 60 Hz frames
    EXPECT_GE(frames, 55); EXPECT_LE(frames, 65);
}

TEST(Device, TheScreenShowsPageSlotUnsavedMarkAndMessages) {
    Rig r;
    r.run_ms(100);
    phasebox::hal::MockHalDisplay text;
    oled::draw(text, r.state, 0, r.presets, r.ms_clock);
    EXPECT_TRUE(text.contains_text("P1: WAVE D1"));
    EXPECT_TRUE(text.contains_text("S1"));
    EXPECT_FALSE(text.contains_text("S1*"));
    EXPECT_TRUE(text.contains_text("VAL: SINE"));

    r.detent(+1);
    r.run_ms(50);
    oled::draw(text, r.state, 0, r.presets, r.ms_clock);
    EXPECT_TRUE(text.contains_text("S1*"));                     // edited, not yet written
    EXPECT_TRUE(text.contains_text("VAL: SAW"));

    r.run_ms(2100);                                             // auto-save kicks in, silently
    oled::draw(text, r.state, 0, r.presets, r.ms_clock);
    EXPECT_FALSE(text.contains_text("S1*"));
    EXPECT_FALSE(text.contains_text("SAVED"));

    r.in.enc_sw = true; r.run_ms(1100);
    oled::draw(text, r.state, 0, r.presets, r.ms_clock);
    EXPECT_TRUE(text.contains_text("SAVED"));
    r.in.enc_sw = false; r.run_ms(60);
}

TEST(Device, TheCurveAndHoldBarAreDrawnOnBothPanelSizes) {
    for (uint16_t h : {uint16_t(32), uint16_t(64)}) {
        Rig r(true, h);
        r.run_ms(100);
        phasebox::hal::FrameBuffer fb(128, h);
        oled::draw(fb, r.state, 0, r.presets, r.ms_clock);
        int lit = 0; for (size_t i = 0; i < fb.size(); ++i) for (int b = 0; b < 8; ++b) lit += (fb.data()[i] >> b) & 1;
        EXPECT_GE(lit, 300);                                    // text, two divider lines and the waveform
        EXPECT_LE(lit, static_cast<int>(fb.size()) * 8 / 2);    // but not a solid block

        r.in.enc_sw = true; r.run_ms(600);                      // hold in progress: bar grows along the top divider
        phasebox::hal::FrameBuffer held(128, h);
        oled::draw(held, r.state, 0, r.presets, r.ms_clock);
        int bar = 0; for (int x = 0; x < 128; ++x) bar += (held.data()[x] >> 2) & 1;   // y = 10 is bit 2 of page 1 -> row below
        (void)bar;
        int on_line = 0; for (int x = 0; x < 128; ++x) on_line += (held.data()[128 + x] >> 2) & 1;   // y=10: page 1, bit 2
        EXPECT_GE(on_line, 60); EXPECT_LE(on_line, 80);          // about 60% of 128 px after 600 of 1000 ms
        r.in.enc_sw = false; r.run_ms(60);
    }
}

TEST(Device, SettingsComeBackAfterAPowerCycle) {
    MemStorage shared;
    {
        PhaseBoxState s; PresetSession p(shared); Recorder m;
        DeviceApp app(s, p, m);
        p.load(s, 0);
        DeviceInputs in; uint64_t t = 1000000;
        auto step = [&](int n) { for (int i = 0; i < n; ++i) { t += 1000; app.poll_inputs(in, t); if (i % 17 == 0) app.frame(t); } };
        step(50);
        static const bool cw[4][2] = {{0,1},{0,0},{1,0},{1,1}};
        for (int i = 0; i < 4; ++i) { in.enc_a = cw[i][0]; in.enc_b = cw[i][1]; step(1); }
        EXPECT_EQ(s.osc.shape, PhaseOscillator::SAW);
        step(2300);                                             // quiet for 2 s: auto-saved
    }
    PhaseBoxState s2; PresetSession p2(shared);
    EXPECT_TRUE(p2.load(s2, 0));
    EXPECT_EQ(s2.osc.shape, PhaseOscillator::SAW);
}

TEST(Device, WithoutStorageEditsStayUnsavedAndHoldSaysSo) {
    Rig r(false);
    r.run_ms(100);
    r.detent(+1);
    r.run_ms(5000);
    EXPECT_TRUE(r.presets.dirty());                             // honest: nothing was kept
    EXPECT_EQ(r.presets.write_attempts(), 0u);                  // and nothing was attempted
    r.in.enc_sw = true; r.run_ms(1100);
    phasebox::hal::MockHalDisplay text;
    oled::draw(text, r.state, 0, r.presets, r.ms_clock);
    EXPECT_TRUE(text.contains_text("NO STORAGE"));
    EXPECT_FALSE(text.contains_text("SAVED"));
    r.in.enc_sw = false; r.run_ms(60);
}

TEST(Device, FirstFrameAfterALongPauseDoesNotJumpTheLfo) {
    Rig r;
    r.run_ms(50);
    const float before = r.state.osc.phase;
    r.app.frame(r.now_us + 60ull * 1000000ull);                 // a minute passes without a frame
    float step = r.state.osc.phase - before; if (step < 0) step += 6.2832f;
    EXPECT_NEAR(step, 1.2566f, 0.05f);                          // dt is capped at 100 ms: at 120 BPM that is 0.2 of a cycle, not 120 cycles
}

TEST(Device, TheScreenSaysFrozenOrBypassAndShowsAFlatLineInBypass) {
    for (uint16_t h : {uint16_t(32), uint16_t(64)}) {
        Rig r(true, h);
        r.run_ms(100);
        phasebox::hal::MockHalDisplay text;
        oled::draw(text, r.state, 0, r.presets, r.ms_clock);
        EXPECT_FALSE(text.contains_text("FROZEN"));
        r.state.frozen = true;
        oled::draw(text, r.state, 0, r.presets, r.ms_clock);
        EXPECT_TRUE(text.contains_text("FROZEN"));
        r.state.bypassed = true;
        oled::draw(text, r.state, 0, r.presets, r.ms_clock);
        EXPECT_TRUE(text.contains_text("BYPASS"));
        EXPECT_FALSE(text.contains_text("FROZEN"));
        phasebox::hal::FrameBuffer fb(128, h);
        oled::draw(fb, r.state, 0, r.presets, r.ms_clock);
        const int y = h - 12;                                    // the bottom row of the curve area: the flat zero line
        int lit = 0; for (int x = 0; x < 128; ++x) lit += (fb.data()[x + (y / 8) * 128] >> (y % 8)) & 1;
        EXPECT_GE(lit, 60);
    }
}
