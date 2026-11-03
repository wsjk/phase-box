#pragma once
#include <cstdint>

/// OLED burn-in protection: dim after 30 s without encoder or footswitch activity, restore on activity.
class ScreenSaver {
public:
    static constexpr uint32_t TIMEOUT_MS = 30000;
    static constexpr uint8_t CONTRAST_NORMAL = 0xCF;
    static constexpr uint8_t CONTRAST_DIM = 0x01;

    /// Returns true when the contrast must be changed; read it with contrast().
    bool update(uint32_t now_ms, uint32_t last_activity_ms) {
        const bool idle = static_cast<uint32_t>(now_ms - last_activity_ms) >= TIMEOUT_MS;
        if (idle == dimmed_) return false;
        dimmed_ = idle;
        return true;
    }
    uint8_t contrast() const { return dimmed_ ? CONTRAST_DIM : CONTRAST_NORMAL; }
    bool dimmed() const { return dimmed_; }

private:
    bool dimmed_ = false;
};
