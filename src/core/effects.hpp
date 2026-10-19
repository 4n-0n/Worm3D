#pragma once
// effects.hpp — potenziamenti attivi con durata. Nessun flag booleano sparso:
// lo stato e' un array di timer indicizzato per EffectKind.

#include "core/food.hpp"

namespace w3 {

struct Effects {
    void advance(float dt);
    void apply(EffectKind effect, float seconds);
    void clear(EffectKind effect);
    void clearAll();
    bool has(EffectKind effect) const;
    float remaining(EffectKind effect) const;

    float timer[kEffectCount] = {};
    float total[kEffectCount] = {};

    // Combinazioni: turbo accelera, lento rallenta e allarga lo sterzo.
    float speedMultiplier() const;
    float turnMultiplier() const;
    float scoreMultiplier() const;
};

}  // namespace w3
