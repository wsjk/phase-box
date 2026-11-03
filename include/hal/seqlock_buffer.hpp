#pragma once
// Hands a block of bytes from one core (writer) to the other (reader) without locks: the reader
// retries if the writer was mid-update, so it never sees a torn frame and the writer never waits.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace phasebox::hal {

template <size_t N>
class SeqlockBuffer {
    static_assert(N % 4 == 0, "size must be a multiple of 4 bytes");

public:
    /// Single writer only.
    void write(const uint8_t* src) {
        const uint32_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);            // odd = write in progress
        std::atomic_thread_fence(std::memory_order_release);
        for (size_t i = 0; i < N / 4; ++i) {
            uint32_t w;
            std::memcpy(&w, src + 4 * i, 4);
            words_[i] = w;
        }
        seq_.store(s + 2, std::memory_order_release);            // even = stable
    }

    /// Returns false if a write was in progress (just try again later).
    bool read(uint8_t* dst, uint32_t* seq_out = nullptr) const {
        const uint32_t s1 = seq_.load(std::memory_order_acquire);
        if (s1 & 1u) return false;
        for (size_t i = 0; i < N / 4; ++i) {
            const uint32_t w = words_[i];
            std::memcpy(dst + 4 * i, &w, 4);
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        if (seq_.load(std::memory_order_relaxed) != s1) return false;
        if (seq_out) *seq_out = s1;
        return true;
    }

private:
    std::atomic<uint32_t> seq_{0};
    volatile uint32_t words_[N / 4] = {};
};

} // namespace phasebox::hal
