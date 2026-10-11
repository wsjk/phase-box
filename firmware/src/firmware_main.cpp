#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "tusb.h"
#include "PhaseOscillator.hpp"
#include "MacroEngine.hpp"

// Core 1 background worker: handles non-blocking UI/OLED tasks
void core1_entry() {
    while (true) {
        tight_loop_contents();
    }
}

// Helper to send standard MIDI Control Change over TinyUSB
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
    tusb_init(); // Initialize TinyUSB stack

    // Launch Core 1 background runner
    multicore_launch_core1(core1_entry);

    PhaseOscillator osc;
    MacroEngine macros;
    
    absolute_time_t next_frame = get_absolute_time();
    const uint32_t dt_us = 16667; // ~60 FPS tick rate

    // Core 0 real-time deterministic DSP & MIDI loop
    while (true) {
        tud_task(); // TinyUSB device task handler (must be called frequently)

        osc.update(0.0166f);

        // Evaluate multi-destination polyrhythmic macros
        auto evaluated_macros = macros.evaluate(osc.phase, 0.0166f, 0.0f);

        // Transmit over USB MIDI if connected
        if (tud_midi_mounted()) {
            for (const auto& m : evaluated_macros) {
                send_usb_midi_cc(m.channel, m.cc, m.value);
            }
        }

        next_frame = delayed_by_us(next_frame, dt_us);
        sleep_until(next_frame);
    }
}
