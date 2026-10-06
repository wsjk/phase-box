#include "core/modulation_engine.hpp"
#include "core/core0_task.hpp"
#include "core/core1_task.hpp"
#include "ui/ui_controller.hpp"
#include "hal/pico_hal_midi.hpp"
#include "hal/pico_hal_gpio.hpp"
#include "hal/pico_hal_display.hpp"
#include "hal/pico_hal_flash.hpp"

#if __has_include("pico/stdlib.h")
#include "pico/stdlib.h"
#include "pico/multicore.h"
#endif

using namespace phasebox::core;
using namespace phasebox::hal;
using namespace phasebox::ui;

// Hardware drivers
static PicoHalMidi g_midi(uart0, 0, 1, 31250);
static PicoHalGpio g_gpio;
static PicoHalDisplay g_display(i2c0, 4, 5, 0x3C, 128, 32);
static PicoHalFlash g_flash;

// Core engine & controllers
static ModulationEngine g_engine;
static Core0Task g_core0_task(g_engine, g_midi, &g_gpio);
static UIController g_ui_controller(g_engine, g_display, &g_flash);
static Core1Task g_core1_task(g_ui_controller, g_gpio);

#if defined(PHASEBOX_PICO_TIMER_AVAILABLE)
static struct repeating_timer g_core0_timer;
#endif

void core1_entry() {
#if __has_include("pico/stdlib.h")
    // Initialize OLED display over I2C at 1 MHz
    g_display.init();

    // Core 1 dedicated loop: UI rendering (30 FPS), encoder menus, and flash writes
    g_core1_task.run();
#endif
}

int main() {
#if __has_include("pico/stdlib.h")
    stdio_init_all();

    // 1. Initialize hardware GPIO (encoder + footswitches)
    g_gpio.init();

    // 2. Initialize RP2040 UART MIDI at 31250 baud
    g_midi.init();

    // 3. Set default tempo (120 BPM)
    g_engine.set_bpm(120.0f);

    // 4. Start 1000 Hz Core 0 timer interrupt loop
    g_core0_task.start_timer(&g_core0_timer);

    // 5. Launch Core 1 for asynchronous UI / OLED rendering and flash management
    multicore_launch_core1(core1_entry);

    // 6. Core 0 background loop
    while (true) {
        tight_loop_contents();
    }
#endif
    return 0;
}
