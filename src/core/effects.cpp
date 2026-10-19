#include "core/effects.hpp"

#include <algorithm>

namespace w3 {
namespace {
int indexOf(EffectKind effect) {
    const int index = static_cast<int>(effect);
    return (index >= 0 && index < kEffectCount) ? index : -1;
}
}  // namespace

void Effects::advance(float dt) {
    for (float& value : timer) {
        if (value > 0.0f) {
            value = std::max(0.0f, value - dt);
        }
    }
}

void Effects::apply(EffectKind effect, float seconds) {
    const int index = indexOf(effect);
    if (index < 0) {
        return;
    }
    // Raccogliere lo stesso frutto rinnova il timer, non lo somma: evita
    // durate fuori controllo con spawn ravvicinati.
    timer[index] = std::max(timer[index], seconds);
    total[index] = std::max(seconds, timer[index]);
}

void Effects::clear(EffectKind effect) {
    const int index = indexOf(effect);
    if (index >= 0) {
        timer[index] = 0.0f;
        total[index] = 0.0f;
    }
}

void Effects::clearAll() {
    for (int i = 0; i < kEffectCount; ++i) {
        timer[i] = 0.0f;
        total[i] = 0.0f;
    }
}

bool Effects::has(EffectKind effect) const {
    const int index = indexOf(effect);
    return index >= 0 && timer[index] > 0.0f;
}

float Effects::remaining(EffectKind effect) const {
    const int index = indexOf(effect);
    return index >= 0 ? timer[index] : 0.0f;
}

float Effects::speedMultiplier() const {
    float value = 1.0f;
    if (has(EffectKind::Turbo)) value *= 1.75f;
    if (has(EffectKind::SlowMo)) value *= 0.55f;
    return value;
}

float Effects::turnMultiplier() const {
    float value = 1.0f;
    if (has(EffectKind::SlowMo)) value *= 1.5f;
    if (has(EffectKind::Turbo)) value *= 0.90f;
    return value;
}

float Effects::scoreMultiplier() const {
    return has(EffectKind::DoubleScore) ? 2.0f : 1.0f;
}

}  // namespace w3
