#pragma once
// Auto-save, hold-to-save and slot handling for the Phase Box presets.
//  - Auto-save: 2 s after the last edit the current settings are written silently.
//  - Hold-to-save: holding the encoder button for 1 s saves at once and shows "SAVED".
//  - Dirty detection compares a fingerprint of the *saved* fields, so any edit counts and
//    non-persisted state (page, pedal, running phase) never causes a write.
// Time is passed in explicitly (milliseconds, wrap-safe), so the same code runs on the device,
// in the simulator and in unit tests with a fake clock.
#include <cstdint>
#include <cstring>
#include "PresetManager.hpp"

class PresetSession {
public:
    enum class Event { None, ShortPress, LongPress };

    static constexpr uint32_t HOLD_MS = 1000;
    static constexpr uint32_t AUTOSAVE_QUIET_MS = 2000;
    static constexpr uint32_t RETRY_MS = 10000;
    static constexpr uint32_t STATUS_MS = 1500;
    static constexpr int MIN_SLOT = 1;
    static constexpr int MAX_SLOT = 8;

    explicit PresetSession(PresetStorage& storage, int slot = 1)
        : storage_(storage), slot_(slot < MIN_SLOT ? MIN_SLOT : (slot > MAX_SLOT ? MAX_SLOT : slot)) {}

    int slot() const { return slot_; }
    uint32_t write_attempts() const { return write_attempts_; }
    /// True if the settings differ from what is stored (as of the last update()).
    bool dirty() const { return has_baseline_ && last_seen_fp_ != saved_fp_; }

    /// Restore the current slot (power-up). A missing preset is silent; a corrupt one reports "BAD PRESET".
    bool load(PhaseBoxState& s, uint32_t now_ms, bool announce = false) {
        bool ok = false;
        size_t n = storage_.read(slot_, buf_, sizeof(buf_));
        if (n == 0) {
            if (announce) set_status("EMPTY", now_ms);
        } else if (PresetManager::decode(buf_, n, s)) {
            ok = true;
            if (announce) set_status("LOADED", now_ms);
        } else {
            set_status("BAD PRESET", now_ms);
        }
        rebase(s);
        return ok;
    }

    /// Switch slot. Unsaved edits are written to the slot being left first. An empty slot keeps the
    /// current settings and stays clean until you edit or hold-save.
    bool select_slot(int slot, PhaseBoxState& s, uint32_t now_ms) {
        if (slot < MIN_SLOT || slot > MAX_SLOT) return false;
        if (slot == slot_) return true;
        uint32_t fp;
        if (has_baseline_ && fingerprint(s, fp) && fp != saved_fp_) write_encoded(fp, now_ms, false);
        slot_ = slot;
        load(s, now_ms, true);
        return true;
    }

    /// Call every frame: runs the auto-save timer.
    void update(const PhaseBoxState& s, uint32_t now_ms) {
        uint32_t fp;
        if (!fingerprint(s, fp)) return;
        if (!has_baseline_) { saved_fp_ = last_seen_fp_ = fp; has_baseline_ = true; return; }
        if (fp != last_seen_fp_) { last_seen_fp_ = fp; last_change_ms_ = now_ms; }  // every edit restarts the timer
        if (fp == saved_fp_) return;
        if (elapsed(now_ms, last_change_ms_) < AUTOSAVE_QUIET_MS) return;
        if (retry_wait_ && elapsed(now_ms, fail_ms_) < RETRY_MS) return;             // don't hammer a failing store
        write_encoded(fp, now_ms, false);
    }

    /// Call every frame with the encoder button level. A press shorter than HOLD_MS reports ShortPress on
    /// release; reaching HOLD_MS saves immediately and reports LongPress once (no ShortPress on release).
    Event button(bool down, const PhaseBoxState& s, uint32_t now_ms) {
        Event ev = Event::None;
        if (down && !was_down_) { down_since_ms_ = now_ms; fired_ = false; }
        if (down && !fired_ && elapsed(now_ms, down_since_ms_) >= HOLD_MS) {
            fired_ = true;
            save_now(s, now_ms);
            ev = Event::LongPress;
        }
        if (!down && was_down_ && !fired_) ev = Event::ShortPress;
        was_down_ = down;
        return ev;
    }

    /// 0..1 progress of the current hold (for a progress bar); 0 when not holding.
    float hold_progress(uint32_t now_ms) const {
        if (!was_down_ || fired_) return 0.0f;
        float p = static_cast<float>(elapsed(now_ms, down_since_ms_)) / static_cast<float>(HOLD_MS);
        return p > 1.0f ? 1.0f : p;
    }

    /// Short message to show on the OLED ("SAVED", "SAVE FAILED", ...) or nullptr.
    const char* status(uint32_t now_ms) const {
        return (status_ && elapsed(now_ms, status_ms_) < STATUS_MS) ? status_ : nullptr;
    }

private:
    static uint32_t elapsed(uint32_t now, uint32_t then) { return static_cast<uint32_t>(now - then); }

    void set_status(const char* text, uint32_t now_ms) { status_ = text; status_ms_ = now_ms; }

    bool fingerprint(const PhaseBoxState& s, uint32_t& fp) {
        enc_len_ = PresetManager::encode(s, buf_, sizeof(buf_));
        if (enc_len_ < 4) return false;
        std::memcpy(&fp, buf_ + enc_len_ - 4, 4);   // the CRC of everything saved
        return true;
    }

    void rebase(const PhaseBoxState& s) {
        uint32_t fp;
        if (fingerprint(s, fp)) { saved_fp_ = last_seen_fp_ = fp; has_baseline_ = true; }
        retry_wait_ = false;
    }

    bool save_now(const PhaseBoxState& s, uint32_t now_ms) {
        uint32_t fp;
        if (!fingerprint(s, fp)) return false;
        return write_encoded(fp, now_ms, true);
    }

    // Writes whatever fingerprint() last encoded into buf_.
    bool write_encoded(uint32_t fp, uint32_t now_ms, bool announce) {
        ++write_attempts_;
        if (storage_.write(slot_, buf_, enc_len_)) {
            saved_fp_ = fp;
            retry_wait_ = false;
            if (announce) set_status("SAVED", now_ms);
            return true;
        }
        retry_wait_ = true;
        fail_ms_ = now_ms;
        set_status("SAVE FAILED", now_ms);
        return false;
    }

    PresetStorage& storage_;
    int slot_;
    uint8_t buf_[PresetManager::MAX_BYTES];
    size_t enc_len_ = 0;

    bool has_baseline_ = false;
    uint32_t saved_fp_ = 0, last_seen_fp_ = 0;
    uint32_t last_change_ms_ = 0;
    bool retry_wait_ = false;
    uint32_t fail_ms_ = 0;
    uint32_t write_attempts_ = 0;

    bool was_down_ = false, fired_ = false;
    uint32_t down_since_ms_ = 0;

    const char* status_ = nullptr;
    uint32_t status_ms_ = 0;
};
