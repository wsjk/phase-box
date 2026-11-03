#pragma once
// Everything the Phase Box does on the device, with the hardware injected so it runs (and is tested)
// on a computer: encoder, switches, pedal in -> pages / tempo / Turing / saving -> MIDI out + OLED frame.
//   Encoder turn   = change the value on the current page (faster turn = bigger BPM steps)
//   Encoder click  = next page            Encoder hold 1 s = save (see PresetSession)
//   Footswitch 1   = tap tempo + restart all LFOs at 0 degrees
//   Footswitch 2   = step the Turing machine
#include <algorithm>
#include <cstdint>
#include "OledRenderer.hpp"
#include "MidiChangeFilter.hpp"
#include "TapTempo.hpp"
#include "hal/debounce.hpp"
#include "hal/frame_buffer.hpp"
#include "hal/pinout.hpp"
#include "hal/quadrature.hpp"

/// Receives MIDI CC messages (the firmware fans them out to USB and the DIN/TRS UART).
class MidiOut {
public:
    virtual ~MidiOut() = default;
    virtual void cc(uint8_t channel, uint8_t controller, uint8_t value) = 0;
};

/// One reading of the physical controls. Switches: true = pressed. Encoder A/B: raw pin levels (1 = high).
struct DeviceInputs {
    bool enc_a = true, enc_b = true;
    bool enc_sw = false, tap = false, mutate = false;
    float expression = 0.0f;   // 0..1, already smoothed
};

class DeviceApp {
public:
    static constexpr uint32_t FRAME_US = 16667;        // engine + MIDI at 60 Hz
    static constexpr uint32_t RENDER_INTERVAL_MS = 33; // OLED at ~30 FPS
    static constexpr uint32_t FAST_TURN_MS = 30;       // detents closer than this count as a fast spin

    DeviceApp(PhaseBoxState& state, PresetSession& presets, MidiOut& midi, uint16_t oled_w = 128, uint16_t oled_h = 32)
        : s_(state), presets_(presets), midi_(midi), fb_(oled_w, oled_h) {}

    /// Call as often as possible (every loop pass): reads the controls.
    void poll_inputs(const DeviceInputs& in, uint64_t now_us) {
        const uint32_t ms = to_ms(now_us);

        const int detent = quad_.feed(in.enc_a, in.enc_b);
        if (detent != 0) {
            const bool fast = have_detent_ && static_cast<uint32_t>(ms - last_detent_ms_) < FAST_TURN_MS;
            have_detent_ = true;
            last_detent_ms_ = ms;
            simui::adjust(s_, active_dest_, s_.active_page, detent, fast);
            touch(ms);
        }

        const bool sw_down = enc_sw_.update(in.enc_sw, ms);
        if (enc_sw_.rose() || enc_sw_.fell()) touch(ms);
        if (presets_.button(sw_down, s_, ms) == PresetSession::Event::ShortPress) {
            s_.active_page = simui::next_page(s_.active_page, +1);
        }

        tap_sw_.update(in.tap, ms);
        if (tap_sw_.rose()) {
            float bpm;
            if (tap_tempo_.tap(ms, bpm)) s_.osc.bpm = bpm;
            s_.restart_phase();
            touch(ms);
        }

        mutate_sw_.update(in.mutate, ms);
        if (mutate_sw_.rose()) {
            s_.step_turing();
            touch(ms);
        }

        s_.adc_expression = std::clamp(in.expression, 0.0f, 1.0f);   // the pedal does not count as screen activity
    }

    /// Call at about FRAME_US intervals. Advances the engine, sends MIDI, runs auto-save and redraws.
    /// Returns true when a new OLED frame is ready in frame_data().
    bool frame(uint64_t now_us) {
        const uint32_t ms = to_ms(now_us);
        float dt = 0.0f;
        if (have_frame_) dt = std::clamp(static_cast<float>(now_us - last_frame_us_) * 1e-6f, 0.0f, 0.1f);
        have_frame_ = true;
        last_frame_us_ = now_us;

        s_.advance(dt);
        const auto messages = s_.evaluateMacros(dt);
        for (size_t i = 0; i < messages.size(); ++i) {
            const auto& m = messages[i];
            if (filter_.should_send(i, static_cast<uint8_t>(m.channel), static_cast<uint8_t>(m.cc), static_cast<uint8_t>(m.value), ms))
                midi_.cc(static_cast<uint8_t>(m.channel), static_cast<uint8_t>(m.cc), static_cast<uint8_t>(m.value));
        }

        presets_.update(s_, ms);

        if (!rendered_ || static_cast<uint32_t>(ms - last_render_ms_) >= RENDER_INTERVAL_MS) {
            oled::draw(fb_, s_, active_dest_, presets_, ms);
            rendered_ = true;
            last_render_ms_ = ms;
            return true;
        }
        return false;
    }

    const uint8_t* frame_data() const { return fb_.data(); }
    size_t frame_size() const { return fb_.size(); }
    uint32_t last_activity_ms() const { return last_activity_ms_; }
    int active_dest() const { return active_dest_; }

private:
    static uint32_t to_ms(uint64_t us) { return static_cast<uint32_t>(us / 1000u); }  // wraps cleanly after ~49 days
    void touch(uint32_t ms) { last_activity_ms_ = ms; }

    PhaseBoxState& s_;
    PresetSession& presets_;
    MidiOut& midi_;
    phasebox::hal::FrameBuffer fb_;

    phasebox::hal::QuadratureDecoder quad_{phasebox::hal::pins::ENC_TRANSITIONS_PER_DETENT, phasebox::hal::pins::ENC_REVERSED};
    phasebox::hal::Debounce enc_sw_, tap_sw_, mutate_sw_;
    TapTempo tap_tempo_;
    MidiChangeFilter filter_;

    int active_dest_ = 0;
    bool have_detent_ = false, have_frame_ = false, rendered_ = false;
    uint32_t last_detent_ms_ = 0, last_render_ms_ = 0, last_activity_ms_ = 0;
    uint64_t last_frame_us_ = 0;
};
