// Guards the HAL interface migration (AdcInput / DisplayInterface), Core1Task wiring and TuringMachine.
#include <gtest/gtest.h>
#include <set>
#include <vector>
#include "core/core0_task.hpp"
#include "core/core1_task.hpp"
#include "hal/mock_hal_adc.hpp"
#include "hal/mock_hal_display.hpp"
#include "hal/hal_adc.hpp"
#include "hal/pico_hal_adc.hpp"
#include "hal/pico_hal_display.hpp"
#include "hal/pinout.hpp"
#include "TuringMachine.hpp"

using namespace phasebox;

namespace {
struct NullMidi : hal::HalMidi {
    void send_cc(uint8_t, uint8_t, uint8_t) override {}
    bool poll_clock_tick() override { return false; }
};
struct ScriptedGpio : hal::HalGpio {
    bool enc_sw = false;
    int32_t delta = 0;
    bool read_button(hal::ButtonId id) override { return id == hal::ButtonId::EncoderSwitch && enc_sw; }
    int32_t read_encoder_delta() override { int32_t d = delta; delta = 0; return d; }
};
struct Counter { int calls = 0; };
void count_render(void* ctx, uint32_t) { ++static_cast<Counter*>(ctx)->calls; }
} // namespace

TEST(HalInterfaces, MockAdcDrivesExpressionMacro) {
    core::ModulationEngine engine;
    NullMidi midi;
    hal::MockHalAdc adc;
    core::Core0Task task(engine, midi, nullptr, &adc);   // Core0Task accepts any AdcInput
    adc.set_channel(0, 0.75f);
    task.step(1000);
    EXPECT_NEAR(adc.read_normalized(), 0.75f, 0.0001f);
    EXPECT_NEAR(adc.read_channel(0), 0.75f, 0.0001f);
}

TEST(HalInterfaces, ConcreteAndPicoAdcAreAdcInputs) {
    hal::HalAdc real;
    hal::AdcInput* a = &real;
    EXPECT_TRUE(a->read_normalized() >= 0.0f);
    hal::PicoHalAdc pico;           // host stub
    hal::AdcInput* b = &pico;
    EXPECT_NEAR(b->read_normalized(), 0.0f, 0.0001f);
}

TEST(HalInterfaces, MockDisplayThroughInterface) {
    hal::MockHalDisplay mock;
    hal::DisplayInterface& d = mock;
    d.clear();
    d.draw_pixel(0, 0, true);
    d.draw_string(0, 0, "BPM");
    d.update();
    EXPECT_EQ(d.get_width(), 128);
    EXPECT_EQ(d.get_height(), 32);
    EXPECT_EQ(mock.get_buffer()[0], 0x01);
    EXPECT_TRUE(mock.contains_text("BPM"));
    EXPECT_EQ(mock.get_update_count(), 1u);
}

TEST(HalInterfaces, PinoutMatchesDocumentedWiring) {
    EXPECT_EQ(hal::pins::OLED_SDA, 4);
    EXPECT_EQ(hal::pins::OLED_SCL, 5);
    EXPECT_EQ(hal::pins::ENC_SW, 6);
    EXPECT_EQ(hal::pins::EXPR_PEDAL, 26);
}

TEST(Core1, EncoderRoutesToUiAndRenderIsThrottled) {
    core::ModulationEngine engine;
    engine.set_bpm(120.0f);
    ui::UIController ui(engine);
    ScriptedGpio gpio;
    core::Core1Task task(ui, gpio);
    Counter counter;
    task.set_render_callback(count_render, &counter);

    gpio.enc_sw = true;                // rising edge -> click
    task.step(1000);
    gpio.enc_sw = false;
    EXPECT_TRUE(ui.get_current_screen() != ui::Screen::DASHBOARD);   // click left the dashboard
    EXPECT_EQ(counter.calls, 1);       // first frame renders immediately

    gpio.delta = 1;
    task.step(10000);                  // < 33 ms later: no render
    EXPECT_EQ(counter.calls, 1);
    task.step(1000 + core::Core1Task::RENDER_INTERVAL_US + 1);
    EXPECT_EQ(counter.calls, 2);
}

TEST(Core1, TapIsNotDoubleHandled) {
    core::ModulationEngine engine;
    engine.set_bpm(120.0f);
    ui::UIController ui(engine);
    ScriptedGpio gpio;
    core::Core1Task task(ui, gpio);
    task.step(1000);
    EXPECT_NEAR(engine.get_bpm(), 120.0f, 0.001f);
}

TEST(Turing, ZeroMutationLoopsEveryLoopLength) {
    TuringMachine t;
    t.mutation_prob = 0.0f;
    for (int i = 0; i < 40; ++i) t.step();
    std::vector<int> a, b;
    for (int i = 0; i < 16; ++i) a.push_back(t.step());
    for (int i = 0; i < 16; ++i) b.push_back(t.step());
    EXPECT_TRUE(a == b);
}

TEST(Turing, FullMutationIsDeterministicNotNoise) {
    // Always flipping the feedback bit gives a fixed pattern (period 2x loop length),
    // so 100% is NOT the most random setting.
    TuringMachine t;
    t.mutation_prob = 1.0f;
    for (int i = 0; i < 40; ++i) t.step();
    std::vector<int> a, b;
    for (int i = 0; i < 32; ++i) a.push_back(t.step());
    for (int i = 0; i < 32; ++i) b.push_back(t.step());
    EXPECT_TRUE(a == b);
}

TEST(Turing, HalfMutationVariesAndSeedIsReproducible) {
    TuringMachine x, y;
    x.mutation_prob = y.mutation_prob = 0.5f;
    x.seed(42); y.seed(42);
    std::set<int> seen;
    bool same = true;
    for (int i = 0; i < 256; ++i) { int a = x.step(); int b = y.step(); seen.insert(a); same = same && (a == b); }
    EXPECT_TRUE(same);
    EXPECT_GE(seen.size(), 40u);
    TuringMachine z; z.mutation_prob = 0.5f; z.seed(0);   // seed(0) must not lock the RNG
    std::set<int> seen0; for (int i = 0; i < 256; ++i) seen0.insert(z.step());
    EXPECT_GE(seen0.size(), 40u);
}
