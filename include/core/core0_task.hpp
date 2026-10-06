#pragma once
#include "core/modulation_engine.hpp"
#include "hal/hal_midi.hpp"
#include "hal/hal_gpio.hpp"
#include <cstdint>

#if __has_include("pico/time.h")
#include "pico/time.h"
#define PHASEBOX_PICO_TIMER_AVAILABLE 1
#endif

namespace phasebox::core {

/**
 * @brief Core 0 Real-Time Task Execution
 * 
 * Runs on Core 0 at 1000 Hz (1 ms interval) via a repeating hardware timer interrupt.
 * Responsible for:
 * 1. Polling MIDI clock bytes from UART / external input.
 * 2. Sampling footswitches (Tap Tempo / Phase Reset & Mutation trigger).
 * 3. Advancing the 4-channel PhaseLFO modulation engine.
 * 4. Dispatching MIDI CC messages over UART via HalMidi.
 */
class Core0Task {
public:
    static constexpr uint32_t TICK_INTERVAL_US = 1000; // 1000 Hz = 1000 microseconds

    Core0Task(ModulationEngine& engine, hal::HalMidi& midi, hal::HalGpio* gpio = nullptr)
        : engine_(engine), midi_(midi), gpio_(gpio) {}

    /**
     * @brief Execute a single 1000 Hz real-time step.
     * @param current_time_us Current system timestamp in microseconds.
     */
    void step(uint32_t current_time_us) {
        // 1. Process any pending external MIDI clock bytes
        while (midi_.poll_clock_tick()) {
            engine_.get_clock_manager().process_midi_clock_byte(current_time_us);
        }

        // 2. Sample tactile footswitch inputs
        if (gpio_ != nullptr) {
            bool tap_pressed = gpio_->read_button(hal::ButtonId::FootswitchTapMode);
            if (tap_pressed && !last_tap_state_) {
                // Footswitch rising edge: Calculate tap tempo & reset LFO phase alignment
                engine_.get_clock_manager().handle_tap_tempo(current_time_us);
                for (size_t i = 0; i < 4; ++i) {
                    engine_.get_lfo(i).reset_phase();
                }
            }
            last_tap_state_ = tap_pressed;

            bool mutate_pressed = gpio_->read_button(hal::ButtonId::FootswitchMutation);
            if (mutate_pressed && !last_mutate_state_) {
                mutate_event_count_++;
            }
            last_mutate_state_ = mutate_pressed;
        }

        // 3. Tick modulation engine (advances accumulators, samples LUTs)
        auto messages = engine_.tick(current_time_us);

        // 4. Dispatch generated MIDI CC messages via HAL
        for (const auto& msg : messages) {
            uint8_t channel = (msg.status & 0x0F) + 1; // 1-indexed MIDI channel
            midi_.send_cc(channel, msg.data1, msg.data2);
        }

        tick_count_++;
        last_step_time_us_ = current_time_us;
    }

    uint32_t get_tick_count() const {
        return tick_count_;
    }

    uint32_t get_last_step_time_us() const {
        return last_step_time_us_;
    }

    uint32_t get_mutate_event_count() const {
        return mutate_event_count_;
    }

    ModulationEngine& get_engine() {
        return engine_;
    }

    hal::HalMidi& get_midi() {
        return midi_;
    }

#if defined(PHASEBOX_PICO_TIMER_AVAILABLE)
    /**
     * @brief RP2040 repeating timer callback to trigger Core 0 tick at 1000 Hz.
     */
    static bool repeating_timer_callback(struct repeating_timer* t) {
        if (t != nullptr && t->user_data != nullptr) {
            auto* task = static_cast<Core0Task*>(t->user_data);
            task->step(time_us_32());
            return true; // Keep repeating
        }
        return false;
    }

    /**
     * @brief Start the 1000 Hz timer interrupt loop on RP2040.
     */
    bool start_timer(struct repeating_timer* timer) {
        // Negative interval ensures continuous fixed-rate 1000 Hz execution
        return add_repeating_timer_us(-static_cast<int64_t>(TICK_INTERVAL_US), 
                                      repeating_timer_callback, 
                                      this, 
                                      timer);
    }
#endif

private:
    ModulationEngine& engine_;
    hal::HalMidi& midi_;
    hal::HalGpio* gpio_{nullptr};

    bool last_tap_state_{false};
    bool last_mutate_state_{false};
    uint32_t tick_count_{0};
    uint32_t last_step_time_us_{0};
    uint32_t mutate_event_count_{0};
};

} // namespace phasebox::core
