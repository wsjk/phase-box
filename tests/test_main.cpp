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

#include "ui/ui_controller.hpp"
#include "core/core1_task.hpp"
#include "hal/mock_hal_display.hpp"
#include "hal/mock_hal_flash.hpp"

using namespace phasebox::ui;
using namespace phasebox::hal;

TEST(UIControllerTest, DefaultStateTelemetryRender) {
    ModulationEngine engine;
    engine.set_bpm(125.0f);
    MockHalDisplay display;
    UIController ui(engine, display);

    EXPECT_EQ(ui.get_current_state(), UIState::Telemetry);
    ui.render(0);

    EXPECT_GT(display.get_update_count(), 0);
    EXPECT_TRUE(display.contains_text("[SINE]"));
    EXPECT_TRUE(display.contains_text("BPM:125"));
}

TEST(UIControllerTest, EncoderNavigationAndParamEdit) {
    ModulationEngine engine;
    MockHalDisplay display;
    UIController ui(engine, display);

    // Initial state: Telemetry
    EXPECT_EQ(ui.get_current_state(), UIState::Telemetry);

    // Short click: press at 1000us, release at 50000us (< 1s threshold)
    ui.handle_input(0, true, 1000);
    ui.handle_input(0, false, 50000);
    EXPECT_EQ(ui.get_current_state(), UIState::ParameterEdit);
    EXPECT_EQ(ui.get_selected_param(), EditParam::TargetCC);

    // Rotate encoder to adjust target CC
    ui.handle_input(2, false, 60000);
    EXPECT_EQ(engine.get_router().generate_cc_message(0, 100).data1, 16); // 14 + 2 = 16

    // Click again to cycle to Channel parameter
    ui.handle_input(0, true, 70000);
    ui.handle_input(0, false, 80000);
    EXPECT_EQ(ui.get_selected_param(), EditParam::Channel);
}

TEST(UIControllerTest, HoldToSavePresetAndLoad) {
    ModulationEngine engine;
    engine.set_bpm(130.0f);
    MockHalDisplay display;
    MockHalFlash flash;
    UIController ui(engine, display, &flash);

    // Configure custom waveform and mutation probability on LFO 0
    engine.get_lfo(0).set_waveform(Waveform::Square);
    engine.get_lfo(0).set_mutation_probability(75);

    // Press encoder down at 0us
    ui.handle_input(0, true, 0);

    // Advance beyond 1 second (1,000,000 us)
    ui.handle_input(0, true, 1000001);
    EXPECT_EQ(ui.get_current_state(), UIState::HoldToSave);

    // Rotate to select slot 2
    ui.handle_input(2, true, 1000050);
    EXPECT_EQ(ui.get_selected_slot(), 2);

    // Release button to trigger flash write
    ui.handle_input(0, false, 1000100);
    EXPECT_EQ(flash.get_save_count(), 1);
    EXPECT_EQ(flash.get_last_saved_slot(), 2);
    EXPECT_EQ(ui.get_current_state(), UIState::Telemetry);

    // Modify engine settings
    engine.set_bpm(90.0f);
    engine.get_lfo(0).set_waveform(Waveform::Sine);
    engine.get_lfo(0).set_mutation_probability(20);
    EXPECT_FLOAT_EQ(engine.get_clock_manager().get_bpm(), 90.0f);

    bool loaded = ui.load_preset(2);
    EXPECT_TRUE(loaded);
    EXPECT_FLOAT_EQ(engine.get_clock_manager().get_bpm(), 130.0f);
    EXPECT_EQ(engine.get_lfo(0).get_waveform(), Waveform::Square);
    EXPECT_EQ(engine.get_lfo(0).get_mutation_probability(), 75);
}

TEST(UIControllerTest, WaveformAndMutationParameterEditing) {
    ModulationEngine engine;
    MockHalDisplay display;
    UIController ui(engine, display);

    // Enter edit mode (click 1: TargetCC)
    ui.handle_input(0, true, 1000);
    ui.handle_input(0, false, 2000);
    EXPECT_EQ(ui.get_selected_param(), EditParam::TargetCC);

    // Click 2: Channel
    ui.handle_input(0, true, 3000);
    ui.handle_input(0, false, 4000);
    EXPECT_EQ(ui.get_selected_param(), EditParam::Channel);

    // Click 3: Waveform
    ui.handle_input(0, true, 5000);
    ui.handle_input(0, false, 6000);
    EXPECT_EQ(ui.get_selected_param(), EditParam::Waveform);

    // Turn encoder to select Triangle (1)
    ui.handle_input(1, false, 7000);
    EXPECT_EQ(engine.get_lfo(0).get_waveform(), Waveform::Triangle);

    ui.render(7000);
    EXPECT_TRUE(display.contains_text("> WAVE: TRI"));

    // Click 4: MutationProb
    ui.handle_input(0, true, 8000);
    ui.handle_input(0, false, 9000);
    EXPECT_EQ(ui.get_selected_param(), EditParam::MutationProb);

    // Turn encoder to adjust mutation prob (+10%)
    ui.handle_input(2, false, 10000);
    EXPECT_EQ(engine.get_lfo(0).get_mutation_probability(), 30); // 20 + 2*5 = 30
}

TEST(UIControllerTest, ClockSyncFeedback) {
    ModulationEngine engine;
    engine.set_bpm(140.0f);
    MockHalDisplay display;
    UIController ui(engine, display);

    ui.trigger_clock_sync_event(100000);
    EXPECT_EQ(ui.get_current_state(), UIState::ClockSync);

    ui.render(100000);
    EXPECT_TRUE(display.contains_text("TAP TEMPO / SYNC"));
    EXPECT_TRUE(display.contains_text("BPM: 140"));

    // After 1.5 seconds, times out back to Telemetry
    ui.handle_input(0, false, 100000 + UIController::CLOCK_SYNC_DISPLAY_TIMEOUT_US + 1000);
    EXPECT_EQ(ui.get_current_state(), UIState::Telemetry);
}

TEST(Core1TaskTest, ThrottlesRenderingTo30FPS) {
    ModulationEngine engine;
    MockHalDisplay display;
    UIController ui(engine, display);
    MockHalGpio gpio;
    Core1Task core1(ui, gpio);

    // Run 105 steps spaced 1 ms apart (104 ms total)
    // 30 FPS = 33.3 ms render interval -> renders at 0ms, 34ms, 68ms, 102ms (4 renders)
    uint32_t time_us = 0;
    for (int i = 0; i < 105; ++i) {
        core1.step(time_us);
        time_us += 1000;
    }

    EXPECT_EQ(core1.get_step_count(), 105);
    EXPECT_EQ(core1.get_render_count(), 4);
    EXPECT_EQ(display.get_update_count(), 4);
}

TEST(Core1TaskTest, PropagatesEncoderInputs) {
    ModulationEngine engine;
    MockHalDisplay display;
    UIController ui(engine, display);
    MockHalGpio gpio;
    Core1Task core1(ui, gpio);

    // Set encoder delta
    gpio.set_encoder_delta(1);
    core1.step(10000);

    // In Telemetry, rotating moves selected LFO
    EXPECT_EQ(ui.get_selected_lfo(), 1);
}

#include "core/macro_engine.hpp"
#include "hal/mock_hal_adc.hpp"

TEST(MacroEngineTest, ExpressionScalingAndWeights) {
    MacroEngine macro;
    macro.set_weight(0, 1.0f); // LFO 0 full positive scaling
    macro.set_weight(1, -1.0f); // LFO 1 inverse scaling

    // Neutral pedal (0.5) -> no offset
    macro.set_expression_value(0.5f);
    EXPECT_EQ(macro.apply(0, 64), 64);
    EXPECT_EQ(macro.apply(1, 64), 64);

    // Toe down (1.0) -> positive offset for LFO 0, negative for LFO 1
    macro.set_expression_value(1.0f);
    EXPECT_EQ(macro.apply(0, 64), 127);
    EXPECT_EQ(macro.apply(1, 64), 0);

    // Heel down (0.0) -> negative offset for LFO 0, positive for LFO 1
    macro.set_expression_value(0.0f);
    EXPECT_EQ(macro.apply(0, 64), 0);
    EXPECT_EQ(macro.apply(1, 64), 127);
}

TEST(MacroEngineTest, ClampingBoundaries) {
    MacroEngine macro;
    macro.set_weight(0, 1.0f);

    // Exceeding top boundary clamps cleanly to 127
    macro.set_expression_value(1.0f);
    EXPECT_EQ(macro.apply(0, 120), 127);

    // Exceeding bottom boundary clamps cleanly to 0
    macro.set_expression_value(0.0f);
    EXPECT_EQ(macro.apply(0, 10), 0);
}

TEST(ModulationEngineTest, NestedCrossModulation) {
    ModulationEngine engine;
    engine.set_bpm(120.0f);

    // Baseline tick without cross-modulation
    auto msgs_baseline = engine.tick(1000);
    uint8_t lfo0_base = msgs_baseline[0].data2;

    // Enable cross-modulation: LFO 1 modulates LFO 0 at depth 1.0
    ModulationEngine engine_nested;
    engine_nested.set_bpm(120.0f);
    engine_nested.set_cross_modulation(1, 0, 1.0f);
    EXPECT_FLOAT_EQ(engine_nested.get_cross_modulation(1, 0), 1.0f);

    auto msgs_nested = engine_nested.tick(1000);
    // Since LFO 1 is at square or sine LUT, its offset influences LFO 0
    EXPECT_NE(msgs_nested[0].data2, 0);
}

TEST(Core0TaskTest, ExpressionPedalViaHalAdc) {
    ModulationEngine engine;
    engine.get_macro_engine().set_weight(0, 1.0f); // LFO 0 responds to expression

    MockHalMidi midi;
    MockHalGpio gpio;
    MockHalAdc adc;
    Core0Task core0(engine, midi, &gpio, &adc);

    // Expression pedal at heel (0.0f)
    adc.set_channel(0, 0.0f);
    core0.step(1000);
    const auto& sent_heel = midi.get_sent_messages();
    ASSERT_GE(sent_heel.size(), 4);
    uint8_t val_heel = sent_heel[0].value;

    // Expression pedal at toe (1.0f)
    midi.clear();
    adc.set_channel(0, 1.0f);
    core0.step(2000);
    const auto& sent_toe = midi.get_sent_messages();
    ASSERT_GE(sent_toe.size(), 4);
    uint8_t val_toe = sent_toe[0].value;

    // Toe position must produce a higher value than heel position
    EXPECT_GT(val_toe, val_heel);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


