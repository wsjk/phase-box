#pragma once
#include <cstdint>
#include <random>
#include <algorithm>

class TuringMachine {
private:
    uint16_t register_bits = 0xACE1;
    uint8_t loop_length = 16;
    std::mt19937 rng;

public:
    float mutation_prob = 0.0f; // Probability of bit mutation [0.0 to 1.0]

    TuringMachine() {
        std::random_device rd;
        rng.seed(rd());
    }

    uint8_t step() {
        bool feedback_bit = (register_bits >> (loop_length - 1)) & 1;
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        if (dist(rng) < mutation_prob) {
            feedback_bit = !feedback_bit;
        }
        register_bits = (register_bits << 1) | feedback_bit;
        uint16_t mask = (1 << loop_length) - 1;
        register_bits &= mask;
        return static_cast<uint8_t>(register_bits & 0x7F);
    }

    void setLoopLength(uint8_t len) {
        loop_length = std::clamp(len, (uint8_t)2, (uint8_t)16);
    }
};
