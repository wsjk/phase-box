#include <gtest/gtest.h>
#include "hal/hal_gpio.hpp"
#include "core/phase_lfo.hpp"
#include "core/clock_manager.hpp"
#include "core/midi_router.hpp"
#include "core/modulation_engine.hpp"

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

#include "core/core0_task.hpp"
#include "hal/mock_hal_midi.hpp"
#include "hal/mock_hal_gpio.hpp"

TEST(ModulationEngineTest, EngineTickGeneratesMessages) {
    ModulationEngine engine;
    engine.set_bpm(120.0f);
    auto msgs = engine.tick(1000);

    EXPECT_EQ(msgs.size(), 4);
    EXPECT_EQ(msgs[0].status, 0xB0); // Accesses first MidiMessage in vector
    EXPECT_EQ(msgs[0].data1, 14); // CC #14 for LFO 0
    EXPECT_EQ(msgs[1].status, 0xB1); // Accesses second MidiMessage in vector
    EXPECT_EQ(msgs[1].data1, 15); // CC #15 for LFO 1
}

TEST(Core0TaskTest, StepDispatchesFourMidiChannels) {
    ModulationEngine engine;
    phasebox::hal::MockHalMidi midi;
    Core0Task core0(engine, midi);

    core0.step(1000);

    EXPECT_EQ(core0.get_tick_count(), 1);
    const auto& sent = midi.get_sent_messages();
    ASSERT_EQ(sent.size(), 4);

    // Channel 1, CC #14
    EXPECT_EQ(sent[0].channel, 1);
    EXPECT_EQ(sent[0].controller, 14);

    // Channel 2, CC #15
    EXPECT_EQ(sent[1].channel, 2);
    EXPECT_EQ(sent[1].controller, 15);

    // Channel 3, CC #16
    EXPECT_EQ(sent[2].channel, 3);
    EXPECT_EQ(sent[2].controller, 16);

    // Channel 4, CC #17
    EXPECT_EQ(sent[3].channel, 4);
    EXPECT_EQ(sent[3].controller, 17);
}

TEST(Core0TaskTest, MidiClockProcessing) {
    ModulationEngine engine;
    phasebox::hal::MockHalMidi midi;
    Core0Task core0(engine, midi);

    uint32_t time_us = 1000000;
    // Inject 25 MIDI clock pulses (24 PPQN = 1 beat, ~20833 us for 120 BPM)
    for (int i = 0; i < 25; ++i) {
        midi.inject_clock_tick();
        core0.step(time_us);
        time_us += 20833;
    }

    EXPECT_EQ(engine.get_clock_manager().get_clock_source(), ClockSource::ExternalMIDI);
    EXPECT_NEAR(engine.get_clock_manager().get_bpm(), 120.0f, 1.0f);
}

TEST(Core0TaskTest, FootswitchTapTempoAndPhaseReset) {
    ModulationEngine engine;
    phasebox::hal::MockHalMidi midi;
    phasebox::hal::MockHalGpio gpio;
    Core0Task core0(engine, midi, &gpio);

    // Advance LFO
    core0.step(1000);

    // First tap at 1,000,000 us
    gpio.set_button(phasebox::hal::ButtonId::FootswitchTapMode, true);
    core0.step(1000000);
    gpio.set_button(phasebox::hal::ButtonId::FootswitchTapMode, false);
    core0.step(1100000);

    // Second tap at 1,500,000 us (delta = 500ms -> 120 BPM)
    gpio.set_button(phasebox::hal::ButtonId::FootswitchTapMode, true);
    core0.step(1500000);
    gpio.set_button(phasebox::hal::ButtonId::FootswitchTapMode, false);

    EXPECT_NEAR(engine.get_clock_manager().get_bpm(), 120.0f, 0.1f);
}

TEST(Core0TaskTest, Continuous1000HzExecution) {
    ModulationEngine engine;
    phasebox::hal::MockHalMidi midi;
    Core0Task core0(engine, midi);

    // Simulate 1000 ticks at 1000 Hz (1 second)
    uint32_t time_us = 0;
    for (int i = 0; i < 1000; ++i) {
        core0.step(time_us);
        time_us += 1000;
    }

    EXPECT_EQ(core0.get_tick_count(), 1000);
    EXPECT_EQ(midi.get_sent_count(), 4000); // 4 messages per 1000 Hz tick
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

