#pragma once
#include <cstdint>

namespace phasebox::hal {

/// Rotary-encoder decoder: feed it the A and B pin levels as often as you can and it returns one
/// step (+1 / -1) per detent.
///   Clockwise = A leads B:  00 -> 10 -> 11 -> 01 -> 00   (reversed = true flips this)
/// Invalid jumps (both pins changed at once, i.e. a missed edge) are ignored and contact bounce that
/// returns to the same position cancels out. With pull-ups an encoder rests at A=B=1.
class QuadratureDecoder {
public:
    explicit QuadratureDecoder(int transitions_per_detent = 4, bool reversed = false)
        : per_detent_(transitions_per_detent < 1 ? 1 : transitions_per_detent), reversed_(reversed) {}

    int feed(bool a, bool b) {
        const uint8_t cur = static_cast<uint8_t>((a ? 2 : 0) | (b ? 1 : 0));
        if (!started_) { state_ = cur; started_ = true; return 0; }
        if (cur == state_) return 0;
        const int8_t t = kTable[(state_ << 2) | cur];
        state_ = cur;
        if (t == 0) return 0;
        acc_ += reversed_ ? -t : t;
        if (acc_ >= per_detent_) { acc_ -= per_detent_; return +1; }
        if (acc_ <= -per_detent_) { acc_ += per_detent_; return -1; }
        if (per_detent_ == 4 && cur == 3) acc_ = 0;   // back at the resting position: forget a half-finished turn
        return 0;
    }

private:
    // index = (previous << 2) | current, state = (A << 1) | B
    static constexpr int8_t kTable[16] = { 0, -1, +1, 0,
                                          +1,  0,  0, -1,
                                          -1,  0,  0, +1,
                                           0, +1, -1, 0 };
    int per_detent_;
    bool reversed_;
    bool started_ = false;
    uint8_t state_ = 3;
    int acc_ = 0;
};

} // namespace phasebox::hal
