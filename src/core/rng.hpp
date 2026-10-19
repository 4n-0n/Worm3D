#pragma once
// rng.hpp — generatore deterministico (xorshift32).
//
// Contratto: stessa sequenza di seed => stessa partita. Serve sia al gioco sia
// alla modalita' --capture, che deve produrre frame riproducibili.

#include <cstdint>

namespace w3 {

struct Rng {
    uint32_t state = 0x9E3779B9u;

    explicit Rng(uint32_t seed = 1u) : state(seed == 0u ? 1u : seed) {}

    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // [0,1)
    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }

    float range(float a, float b) { return a + (b - a) * unit(); }

    int rangeInt(int a, int b) {
        if (b <= a) {
            return a;
        }
        return a + static_cast<int>(next() % static_cast<uint32_t>(b - a + 1));
    }

    bool chance(float probability) { return unit() < probability; }
};

}  // namespace w3
