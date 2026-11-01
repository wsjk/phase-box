#pragma once
#include <cstdint>
#include <algorithm>

class TuringMachine {
private:
    uint16_t register_bits = 0xACE1;
    uint8_t loop_length = 16;
    uint32_t rng_state = 0x1234ABCDu;   // xorshift32; never 0

    uint32_t next_random() {
        rng_state ^= rng_state << 13;
        rng_state ^= rng_state >> 17;
        rng_state ^= rng_state << 5;
        return rng_state;
    }

public:
    float mutation_prob = 0.0f; // Probability of bit mutation [0.0 to 1.0]

    TuringMachine() = default;

    /// Seed the mutation RNG (e.g. from time_us_32() on the device). 0 is mapped to a valid state.
    void seed(uint32_t s) { rng_state = s ? s : 0x1234ABCDu; }

    uint8_t step() {
        bool feedback_bit = (register_bits >> (loop_length - 1)) & 1;
        // 24-bit uniform value in [0,1): no <random>, no heap, no exceptions.
        float u = static_cast<float>(next_random() >> 8) * (1.0f / 16777216.0f);
        if (u < mutation_prob) {
            feedback_bit = !feedback_bit;
        }
        register_bits = static_cast<uint16_t>((register_bits << 1) | feedback_bit);
        uint16_t mask = static_cast<uint16_t>((1u << loop_length) - 1u);
        register_bits &= mask;
        return static_cast<uint8_t>(register_bits & 0x7F);
    }

    void setLoopLength(uint8_t len) {
        loop_length = std::clamp(len, (uint8_t)2, (uint8_t)16);
    }
};
