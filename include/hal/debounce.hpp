#pragma once
#include <cstdint>

namespace phasebox::hal {

/// Debounces a mechanical switch: the output only changes after the input has stayed the same for
/// `settle_ms`. Feed it the *pressed* level (true = pressed) and a wrap-safe millisecond clock.
class Debounce {
public:
    explicit Debounce(uint32_t settle_ms = 20) : settle_ms_(settle_ms) {}

    bool update(bool raw, uint32_t now_ms) {
        rose_ = fell_ = false;
        if (raw != candidate_) { candidate_ = raw; since_ms_ = now_ms; }
        if (candidate_ != stable_ && static_cast<uint32_t>(now_ms - since_ms_) >= settle_ms_) {
            stable_ = candidate_;
            if (stable_) rose_ = true; else fell_ = true;
        }
        return stable_;
    }

    bool state() const { return stable_; }
    bool rose() const { return rose_; }   // became pressed during the last update()
    bool fell() const { return fell_; }   // became released during the last update()

private:
    uint32_t settle_ms_;
    bool candidate_ = false, stable_ = false, rose_ = false, fell_ = false;
    uint32_t since_ms_ = 0;
};

} // namespace phasebox::hal
