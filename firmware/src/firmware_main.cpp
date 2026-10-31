#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "PhaseOscillator.hpp"
#include "MacroEngine.hpp"
#include "PhaseBoxState.hpp"
#include "PresetManager.hpp"
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
    
    // Attempt to load default preset slot 1 on boot
    if (!PresetManager::loadPreset(1, state)) {
        // Fallback default save if no preset exists yet
        PresetManager::savePreset(1, state);
    }

    absolute_time_t next_frame = get_absolute_time();
    const uint32_t dt_us = 16667;
    
    uint32_t auto_save_counter = 0;
    bool is_dirty = false;

    while (true) {
        tud_task();

        state.osc.update(0.0166f);
        auto evaluated_macros = state.macros.evaluate(state.osc.phase, 0.0166f, state.adc_expression);

        if (tud_midi_mounted()) {
            for (const auto& m : evaluated_macros) {
                send_usb_midi_cc(m.channel, m.cc, m.value);
            }
        }

        // Background auto-save dirty state watcher (commits 2s after last tweak)
        if (is_dirty) {
            auto_save_counter++;
            if (auto_save_counter > 120) { // ~2 seconds at 60Hz
                PresetManager::savePreset(1, state);
                is_dirty = false;
                auto_save_counter = 0;
            }
        }

        next_frame = delayed_by_us(next_frame, dt_us);
        sleep_until(next_frame);
    }
}
