#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "PhaseOscillator.hpp"
#include "MacroEngine.hpp"
#include "PhaseBoxState.hpp"
#include "PresetManager.hpp"
#include "PresetSession.hpp"
#include "screensaver.hpp"

// Core 1 background worker: handles non-blocking UI/OLED tasks and burn-in protection
void core1_entry() {
    core1_ui_loop();
}

void send_usb_midi_cc(uint8_t channel, uint8_t cc, uint8_t value) {
    uint8_t msg[3] = {
        (uint8_t)(0xB0 | ((channel - 1) & 0x0F)),
        (uint8_t)(cc & 0x7F),
        (uint8_t)(value & 0x7F)
    };
    tud_midi_stream_write(0, msg, 3);
}

int main() {
    stdio_init_all();
    board_init();
    tusb_init();

    multicore_launch_core1(core1_entry);

    PhaseBoxState state;
    state.turing.seed(time_us_32()); // different generative sequence each boot
    
    // Presets: restore slot 1 on boot, auto-save 2 s after the last change (see PresetSession).
    // NOTE: FilePresetStorage needs a filesystem, which the RP2040 build does not have yet, so nothing
    // persists on the device until a flash-backed PresetStorage is plugged in here.
    FilePresetStorage preset_storage;
    PresetSession presets(preset_storage);
    presets.load(state, to_ms_since_boot(get_absolute_time()));

    absolute_time_t next_frame = get_absolute_time();
    const uint32_t dt_us = 16667;

    while (true) {
        tud_task();

        state.advance(0.0166f);
        auto evaluated_macros = state.evaluateMacros(0.0166f);

        if (tud_midi_mounted()) {
            for (const auto& m : evaluated_macros) {
                send_usb_midi_cc(m.channel, m.cc, m.value);
            }
        }

        // Auto-save watcher: any change to a saved setting restarts a 2 s timer, then it writes.
        presets.update(state, to_ms_since_boot(get_absolute_time()));

        next_frame = delayed_by_us(next_frame, dt_us);
        sleep_until(next_frame);
    }
}
