#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/uart.h"
#include "hardware/timer.h"

#include "core/modulation_engine.hpp"
#include "core/macro_engine.hpp"
#include "hal/hal_adc.hpp"
#include "hal/hal_encoder.hpp"
#include "hal/hal_display.hpp"
#include "storage/preset_manager.hpp"
#include "ui/ui_controller.hpp"

// UART Configuration for Standard MIDI (31,250 baud)
#define MIDI_UART_ID   uart0
#define MIDI_BAUD_RATE 31250
#define MIDI_TX_PIN    0
#define MIDI_RX_PIN    1

using namespace phasebox::core;
using namespace phasebox::hal;
using namespace phasebox::storage;
using namespace phasebox::ui;

// Global Instances for Dual-Core Operations
static ModulationEngine g_engine;
static MacroEngine      g_macro;
static HalAdc           g_adc;

static HalEncoder       g_encoder;
static HalDisplay       g_display;
static phasebox::ui::UIController       g_ui_controller(g_engine);

// Forward declaration for Core 1 UI loop
void core1_main();

/**
 * @brief Core 0 Hardware Timer Callback (1000 Hz / 1ms period)
 * Driven by the RP2040 hardware timer alarm.
 */
bool repeating_timer_callback(struct repeating_timer *t) {
    uint32_t current_time_us = time_us_32();

    // 1. Read smoothed expression pedal ADC voltage [0.0f - 1.0f]
    float expr_val = g_adc.read_normalized();
    g_macro.set_expression_value(expr_val);

    // 2. Tick engine (updates LFO phase accumulators, tap clock, and CC mapping)
    auto msgs = g_engine.tick(current_time_us);

    // 3. Apply macro scaling matrix & transmit MIDI CC over UART0
    for (size_t i = 0; i < msgs.size(); ++i) {
        uint8_t scaled_data2 = g_macro.apply(i, msgs[i].data2);

        uart_putc_raw(MIDI_UART_ID, msgs[i].status);
        uart_putc_raw(MIDI_UART_ID, msgs[i].data1);
        uart_putc_raw(MIDI_UART_ID, scaled_data2);
    }

    return true; // Keep timer repeating
}

int main() {
    // 1. Initialize Pico stdio and HAL drivers
    stdio_init_all();

    // 2. Configure UART0 for 31,250 baud MIDI
    uart_init(MIDI_UART_ID, MIDI_BAUD_RATE);
    gpio_set_function(MIDI_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MIDI_RX_PIN, GPIO_FUNC_UART);

    // 3. Initialize ADC Hardware
    g_adc.init();

    // 4. Launch Core 1 for OLED display, rotary encoder, and flash storage
    multicore_launch_core1(core1_main);

    // 5. Set initial engine parameters
    g_engine.set_bpm(120.0f);

    // 6. Attach 1000 Hz repeating timer interrupt to Core 0 (-1000 us = fixed frequency)
    struct repeating_timer timer;
    add_repeating_timer_us(-1000, repeating_timer_callback, NULL, &timer);

    // 7. Core 0 Idle Loop: Process non-blocking incoming MIDI bytes (e.g. MIDI Clock 0xF8)
    while (true) {
        if (uart_is_readable(MIDI_UART_ID)) {
            uint8_t byte = uart_getc(MIDI_UART_ID);
            if (byte == 0xF8) {
                // Incoming MIDI Clock pulse (24 PPQN sync handling)
            }
        }
        tight_loop_contents();
    }

    return 0;
}

/**
 * @brief Core 1 Entry Point
 * Handles display rendering, rotary menu state transitions, and flash saves.
 */
void core1_main() {
    // Initialize Core 1 peripherals
    g_encoder.init(2, 3, 4); // Pins 2 (CLK), 3 (DT), 4 (SW)
    g_display.init(6, 7);    // Pins 6 (SDA), 7 (SCL)
    g_ui_controller.init();

    while (true) {
        uint32_t now_us = time_us_32();

        // 1. Poll rotary encoder hardware
        g_encoder.update(now_us);
        int delta = g_encoder.get_delta();
        bool btn = g_encoder.is_button_pressed();
        uint32_t hold_ms = g_encoder.get_button_hold_time_ms();

        // 2. Update UI state machine
        g_ui_controller.handle_bpm_encoder_input(delta, time_us_32());

        // 3. Render frame to local display framebuffer and send over I2C
        g_display.clear();
        // g_ui_controller.render();
        g_display.render();

        // 4. Limit to ~30 FPS (33 ms frame delay)
        sleep_ms(33);
    }
}
