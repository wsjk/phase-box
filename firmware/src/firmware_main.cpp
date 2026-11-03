// Phase Box firmware (RP2040).
//   Core 0: USB (TinyUSB), reads the controls, runs the engine at 60 Hz, sends MIDI over USB and DIN/TRS.
//   Core 1: owns the OLED I2C bus. It only copies finished frames to the panel and dims the screen when idle.
// All behaviour lives in DeviceApp (include/DeviceApp.hpp), which is unit-tested on a computer; this file
// only touches pins and peripherals.
#include <atomic>
#include <cstdint>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "DeviceApp.hpp"
#include "PhaseBoxState.hpp"
#include "PresetManager.hpp"
#include "PresetSession.hpp"
#include "ScreenSaver.hpp"
#include "hal/hal_adc.hpp"
#include "hal/pico_hal_display.hpp"
#include "hal/pico_hal_midi.hpp"
#include "hal/pinout.hpp"
#include "hal/seqlock_buffer.hpp"

// OLED rows: 32 or 64 (set with -DPHASEBOX_OLED_HEIGHT=64, or cmake -DPHASEBOX_OLED_HEIGHT=64).
#ifndef PHASEBOX_OLED_HEIGHT
#define PHASEBOX_OLED_HEIGHT 32
#endif

namespace {

constexpr uint16_t OLED_W = 128;
constexpr uint16_t OLED_H = PHASEBOX_OLED_HEIGHT;
constexpr size_t FRAME_BYTES = static_cast<size_t>(OLED_W) * OLED_H / 8;
constexpr uint32_t ADC_INTERVAL_US = 5000;   // pedal read at 200 Hz

phasebox::hal::SeqlockBuffer<FRAME_BYTES> g_shared_frame;   // Core 0 -> Core 1
std::atomic<uint32_t> g_last_activity_ms{0};                // Core 0 -> Core 1

phasebox::hal::PicoHalMidi g_uart_midi;   // uart0: GP0 (TX), GP1 (RX), 31250 baud
phasebox::hal::HalAdc g_adc;              // GP26 / ADC0, smoothed
PhaseBoxState g_state;
NoStorage g_storage;                      // TODO: a flash-backed PresetStorage makes presets survive power-off
PresetSession g_presets(g_storage);

/// Sends every CC to the USB host (when connected) and out of the DIN/TRS jack.
class DualMidiOut : public MidiOut {
public:
    void cc(uint8_t channel, uint8_t controller, uint8_t value) override {
        if (tud_midi_mounted()) {
            uint8_t msg[3] = {static_cast<uint8_t>(0xB0 | ((channel - 1) & 0x0F)),
                              static_cast<uint8_t>(controller & 0x7F),
                              static_cast<uint8_t>(value & 0x7F)};
            tud_midi_stream_write(0, msg, 3);
        }
        g_uart_midi.send_cc(channel, controller, value);
    }
};
DualMidiOut g_midi_out;
DeviceApp g_app(g_state, g_presets, g_midi_out, OLED_W, OLED_H);

uint32_t ms_since_boot() { return to_ms_since_boot(get_absolute_time()); }

void core1_entry() {
    namespace pins = phasebox::hal::pins;
    phasebox::hal::PicoHalDisplay display(i2c0, pins::OLED_SDA, pins::OLED_SCL,
                                          phasebox::hal::PicoHalDisplay::DEFAULT_I2C_ADDR, OLED_W, OLED_H);
    display.init();
    display.set_contrast(ScreenSaver::CONTRAST_NORMAL);

    ScreenSaver saver;
    static uint8_t frame[FRAME_BYTES];
    uint32_t last_seq = 0xFFFFFFFFu;
    for (;;) {
        uint32_t seq;
        if (g_shared_frame.read(frame, &seq) && seq != last_seq) {   // only send frames that changed
            display.load_frame(frame, sizeof(frame));
            display.update();
            last_seq = seq;
        }
        if (saver.update(ms_since_boot(), g_last_activity_ms.load(std::memory_order_relaxed))) {
            display.set_contrast(saver.contrast());
        }
        sleep_ms(5);
    }
}

void init_input_pins() {
    namespace pins = phasebox::hal::pins;
    const unsigned list[] = {pins::ENC_A, pins::ENC_B, pins::ENC_SW, pins::FS_TAP, pins::FS_MUTATE};
    for (unsigned p : list) {
        gpio_init(p);
        gpio_set_dir(p, GPIO_IN);
        gpio_pull_up(p);   // all controls switch to ground
    }
}

} // namespace

int main() {
    namespace pins = phasebox::hal::pins;

    stdio_init_all();
    board_init();
    tusb_init();

    g_uart_midi.init();
    g_adc.init();
    init_input_pins();

    g_state.turing.seed(time_us_32());   // a different Turing sequence each power-up
    g_presets.load(g_state, ms_since_boot());

    multicore_launch_core1(core1_entry);

    float expression = 0.0f;
    uint64_t next_adc_us = time_us_64();
    uint64_t next_frame_us = time_us_64();

    for (;;) {
        tud_task();                                   // USB service on every pass
        const uint64_t now_us = time_us_64();

        if (now_us >= next_adc_us) {
            expression = g_adc.read_normalized();
            next_adc_us = now_us + ADC_INTERVAL_US;
        }

        DeviceInputs in;
        in.enc_a = gpio_get(pins::ENC_A);
        in.enc_b = gpio_get(pins::ENC_B);
        in.enc_sw = !gpio_get(pins::ENC_SW);          // active low
        in.tap = !gpio_get(pins::FS_TAP);
        in.mutate = !gpio_get(pins::FS_MUTATE);
        in.expression = expression;
        g_app.poll_inputs(in, now_us);                // as often as possible so no encoder edge is missed

        if (now_us >= next_frame_us) {
            next_frame_us += DeviceApp::FRAME_US;
            if (now_us > next_frame_us + 100000) next_frame_us = now_us + DeviceApp::FRAME_US;   // fell behind: resync
            if (g_app.frame(now_us)) g_shared_frame.write(g_app.frame_data());
            g_last_activity_ms.store(g_app.last_activity_ms(), std::memory_order_relaxed);
        }
    }
}
