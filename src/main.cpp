#include "core/modulation_engine.hpp"
#include "core/core0_task.hpp"
#include "hal/pico_hal_midi.hpp"

#if __has_include("pico/stdlib.h")
#include "pico/stdlib.h"
#include "pico/multicore.h"
#endif

using namespace phasebox::core;
using namespace phasebox::hal;

// Global hardware instances for RP2040 Core 0
static PicoHalMidi g_midi(uart0, 0, 1, 31250);
static ModulationEngine g_engine;
static Core0Task g_core0_task(g_engine, g_midi);

#if defined(PHASEBOX_PICO_TIMER_AVAILABLE)
static struct repeating_timer g_core0_timer;
#endif

void core1_entry() {
    // Core 1: Dedicated to UI rendering (OLED), Rotary Encoder menus, and Flash preset saves
    while (true) {
#if __has_include("pico/stdlib.h")
        tight_loop_contents();
#endif
    }
}

int main() {
#if __has_include("pico/stdlib.h")
    stdio_init_all();

    // 1. Initialize RP2040 UART MIDI at 31250 baud
    g_midi.init();

    // 2. Set default tempo (120 BPM)
    g_engine.set_bpm(120.0f);

    // 3. Start 1000 Hz Core 0 timer interrupt loop
    g_core0_task.start_timer(&g_core0_timer);

    // 4. Launch Core 1 for asynchronous UI / OLED rendering
    multicore_launch_core1(core1_entry);

    // 5. Core 0 background loop
    while (true) {
        tight_loop_contents();
    }
#endif
    return 0;
}
