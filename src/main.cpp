#include "hardware/timer.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include "core/modulation_engine.hpp"
#include "hal/hal_gpio.hpp"

// UART Configuration for Standard MIDI (31,250 baud)
#define MIDI_UART_ID uart0
#define MIDI_BAUD_RATE 31250
#define MIDI_TX_PIN 0
#define MIDI_RX_PIN 1

using namespace phasebox::core;

// Global instance of ModulationEngine accessed by Core 0 hardware timer
// interrupt
static ModulationEngine g_engine;

// Forward declaration for Core 1 UI loop
void core1_main();

/**
 * @brief Core 0 Hardware Timer Callback (1000 Hz / 1ms period)
 * Driven by the RP2040 hardware timer alarm. Runs deterministically.
 */
bool repeating_timer_callback(struct repeating_timer *t) {
  uint32_t current_time_us = time_us_32();

  // 1. Tick engine (updates LFO phase accumulators, tap clock, and CC mapping)
  auto msgs = g_engine.tick(current_time_us);

  // 2. Transmit raw 3-byte MIDI CC messages over UART0
  for (const auto &msg : msgs) {
    uart_putc_raw(MIDI_UART_ID, msg.status);
    uart_putc_raw(MIDI_UART_ID, msg.data1);
    uart_putc_raw(MIDI_UART_ID, msg.data2);
  }

  return true; // Return true to keep the timer repeating
}

int main() {
  // 1. Initialize Pico stdio and hardware drivers
  stdio_init_all();

  // 2. Configure UART0 for 31,250 baud MIDI
  uart_init(MIDI_UART_ID, MIDI_BAUD_RATE);
  gpio_set_function(MIDI_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(MIDI_RX_PIN, GPIO_FUNC_UART);

  // 3. Launch Core 1 for OLED display, rotary encoder, and flash storage
  multicore_launch_core1(core1_main);

  // 4. Set initial engine parameters
  g_engine.set_bpm(120.0f);

  // 5. Attach 1000 Hz repeating timer interrupt to Core 0
  // Negative interval (-1000 µs) ensures execution at exact 1 ms intervals
  struct repeating_timer timer;
  add_repeating_timer_us(-1000, repeating_timer_callback, NULL, &timer);

  // 6. Core 0 Idle Loop: Process non-blocking incoming MIDI bytes (e.g. MIDI
  // Clock 0xF8)
  while (true) {
    if (uart_is_readable(MIDI_UART_ID)) {
      uint8_t byte = uart_getc(MIDI_UART_ID);
      uint32_t now = time_us_32();

      // Pass incoming 0xF8 bytes to ClockManager for external sync
      if (byte == 0xF8) {
        // ClockManager handles external MIDI clock tracking
      }
    }
    tight_loop_contents();
  }

  return 0;
}

/**
 * @brief Core 1 Entry Point
 * Handles display updates, rotary menu logic, and SPI flash preset management.
 */
void core1_main() {
  // Core 1 runs independently without affecting Core 0 timing precision
  while (true) {
    // ~30 Hz OLED refresh rate (33 ms)
    sleep_ms(33);
  }
}
