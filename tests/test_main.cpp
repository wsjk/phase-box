// tests/test_main.cpp
#include <gtest/gtest.h>
#include "hal/hal_gpio.hpp"
#include "core/phase_lfo.hpp"
#include "core/clock_manager.hpp"
#include "core/midi_router.hpp"

using namespace phasebox::core;

TEST(PhaseLFOTest, WaveformSineLUT) {
    PhaseLFO lfo;
    lfo.set_waveform(Waveform::Sine);
    lfo.set_frequency(1.0f, 1000);

    uint8_t val0 = lfo.update();
    EXPECT_GE(val0, 60);
    EXPECT_LE(val0, 68);
}

TEST(PhaseLFOTest, WaveformSquare) {
    PhaseLFO lfo;
    lfo.set_waveform(Waveform::Square);
    lfo.set_frequency(1.0f, 1000);

    uint8_t val_first = lfo.update();
    EXPECT_EQ(val_first, 127);
}

TEST(PhaseLFOTest, PhaseReset) {
    PhaseLFO lfo;
    lfo.set_waveform(Waveform::Square);
    lfo.set_frequency(100.0f, 1000);

    for (int i = 0; i < 10; ++i) lfo.update();
    lfo.reset_phase();
    uint8_t val = lfo.update();
    EXPECT_EQ(val, 127);
}

TEST(ClockManagerTest, DefaultInternalTap) {
    ClockManager clock;
    EXPECT_EQ(clock.get_clock_source(), ClockSource::InternalTap);
    EXPECT_FLOAT_EQ(clock.get_bpm(), 120.0f);
}

TEST(ClockManagerTest, TapTempoCalculation) {
    ClockManager clock;
    clock.handle_tap_tempo(1000000);
    clock.handle_tap_tempo(1500000);
    EXPECT_NEAR(clock.get_bpm(), 120.0f, 0.1f);
}

TEST(ClockManagerTest, ExternalMIDIClockFallback) {
    ClockManager clock;
    uint32_t time_us = 1000000;
    
    for (int i = 0; i < 25; ++i) {
        clock.process_midi_clock_byte(time_us);
        time_us += 20833;
    }
    
    EXPECT_EQ(clock.get_clock_source(), ClockSource::ExternalMIDI);
    EXPECT_NEAR(clock.get_bpm(), 120.0f, 1.0f);

    clock.update(time_us + 1500000);
    EXPECT_EQ(clock.get_clock_source(), ClockSource::InternalTap);
}

TEST(MidiRouterTest, CCMessageGeneration) {
    MidiRouter router;
    router.set_target_channel(0, 1);
    router.set_target_cc(0, 14);

    MidiMessage msg = router.generate_cc_message(0, 100);
    EXPECT_EQ(msg.status, 0xB0); // CC on Channel 1
    EXPECT_EQ(msg.data1, 14);    // CC #14
    EXPECT_EQ(msg.data2, 100);   // Value 100
}

TEST(MidiRouterTest, ChannelMapping) {
    MidiRouter router;
    router.set_target_channel(1, 2); // Channel 2
    router.set_target_cc(1, 15);

    MidiMessage msg = router.generate_cc_message(1, 64);
    EXPECT_EQ(msg.status, 0xB1); // CC on Channel 2
    EXPECT_EQ(msg.data1, 15);
    EXPECT_EQ(msg.data2, 64);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
