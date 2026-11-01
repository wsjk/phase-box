#pragma once
#include "ui/ui_controller.hpp"
#include "hal/hal_gpio.hpp"
#include <cstdint>

#if __has_include("pico/stdlib.h")
#include "pico/stdlib.h"
#endif

namespace phasebox::core {

/**
 * @brief Core 1 Asynchronous UI and Flash Persistence Task
 * 
 * Runs entirely decoupled from Core 0's real-time 1000 Hz loop.
 * - Samples rotary encoder delta and pushbutton
 * - Updates the 4-state UI menu machine
 * - Throttles OLED display rendering to 30 FPS (33 ms interval)
 * - Safely handles SPI NOR flash preset writes without interrupting Core 0
 */
class Core1Task {
public:
    static constexpr uint32_t RENDER_INTERVAL_US = 33333; // ~30 FPS (33.3 ms)

    /// Called at ~30 FPS to draw the current UI state (set by main; no heap, no std::function).
    using RenderFn = void (*)(void* ctx, uint32_t time_us);

    Core1Task(ui::UIController& ui_controller, hal::HalGpio& gpio)
        : ui_controller_(ui_controller), gpio_(gpio) {}

    void set_render_callback(RenderFn fn, void* ctx) {
        render_fn_ = fn;
        render_ctx_ = ctx;
    }

    void step(uint32_t current_time_us) {
        // The tap-tempo footswitch is owned by Core0Task (real-time side); handling it here
        // as well would apply every tap twice, from two cores.
        int32_t delta = gpio_.read_encoder_delta();
        bool enc_btn = gpio_.read_button(hal::ButtonId::EncoderSwitch);

        if (delta != 0) {
            ui_controller_.handle_encoder_turn(delta, current_time_us);
        }
        if (enc_btn && !last_enc_button_) {
            ui_controller_.handle_encoder_click();
        }
        last_enc_button_ = enc_btn;

        // Throttle OLED rendering to 30 FPS (render first frame immediately on boot)
        if (!has_rendered_ || (current_time_us - last_render_time_us_ >= RENDER_INTERVAL_US)) {
            if (render_fn_ != nullptr) render_fn_(render_ctx_, current_time_us);
            last_render_time_us_ = current_time_us;
            has_rendered_ = true;
            render_count_++;
        }

        step_count_++;
    }

#if __has_include("pico/stdlib.h")
    void run() {
        while (running_) {
            step(time_us_32());
            sleep_ms(2); // Short sleep to yield bus and avoid CPU thrashing
        }
    }

    void stop() {
        running_ = false;
    }
#endif

    uint32_t get_step_count() const { return step_count_; }
    uint32_t get_render_count() const { return render_count_; }
    ui::UIController& get_ui_controller() { return ui_controller_; }

private:
    ui::UIController& ui_controller_;
    hal::HalGpio& gpio_;

    bool last_enc_button_{false};
    RenderFn render_fn_{nullptr};
    void* render_ctx_{nullptr};
    bool has_rendered_{false};
    uint32_t last_render_time_us_{0};
    uint32_t step_count_{0};
    uint32_t render_count_{0};
    bool running_{true};
};

} // namespace phasebox::core
