#pragma once
#include <algorithm>
#include <cstdint>

/// Tap-tempo: the average of the last four tap intervals. A gap longer than MAX_GAP_MS starts over.
class TapTempo {
public:
    static constexpr uint32_t MAX_GAP_MS = 2500;
    static constexpr int MAX_TAPS = 4;

    explicit TapTempo(float min_bpm = 40.0f, float max_bpm = 300.0f) : min_bpm_(min_bpm), max_bpm_(max_bpm) {}

    /// Register a tap. Returns true (and sets `bpm`) once there are at least two taps in a row.
    bool tap(uint32_t now_ms, float& bpm) {
        if (count_ > 0 && static_cast<uint32_t>(now_ms - times_[count_ - 1]) > MAX_GAP_MS) count_ = 0;
        if (count_ == MAX_TAPS) {
            for (int i = 1; i < MAX_TAPS; ++i) times_[i - 1] = times_[i];
            --count_;
        }
        times_[count_++] = now_ms;
        if (count_ < 2) return false;
        const uint32_t span = static_cast<uint32_t>(times_[count_ - 1] - times_[0]);
        const uint32_t avg_ms = span / static_cast<uint32_t>(count_ - 1);
        if (avg_ms == 0) return false;
        bpm = std::clamp(60000.0f / static_cast<float>(avg_ms), min_bpm_, max_bpm_);
        return true;
    }

    void reset() { count_ = 0; }

private:
    float min_bpm_, max_bpm_;
    uint32_t times_[MAX_TAPS] = {};
    int count_ = 0;
};
