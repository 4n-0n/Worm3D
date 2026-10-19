#include "core/food.hpp"

#include <array>

namespace w3 {
namespace {

// Ordine = ordine di FoodKind. Ogni riga e' autosufficiente: nome, look,
// punteggio, crescita, effetto, durata, vita e probabilita' di spawn.
constexpr std::array<FoodDef, kFoodKindCount> kFoods{{
    // Mela: il cibo base. Cresce di 1, 10 punti, non scade.
    {FoodKind::Apple, "Mela", "MELA", {0.95f, 0.18f, 0.20f}, {1.0f, 0.35f, 0.25f}, 0.18f, 0.55f,
     {1.0f, 1.0f, 1.0f}, MeshShape::Sphere, 10, 1.0f, EffectKind::None, 0.0f, 0.0f, 34.0f},

    // Mela d'oro: rara, scade, vale tanto.
    {FoodKind::GoldenApple, "Mela d'oro", "ORO", {1.0f, 0.80f, 0.22f}, {1.0f, 0.85f, 0.35f}, 0.95f, 0.50f,
     {1.0f, 1.0f, 1.0f}, MeshShape::Sphere, 75, 2.0f, EffectKind::None, 0.0f, 16.0f, 5.0f},

    // Peperoncino: turbo per 6 s. Ellissoide allungato.
    {FoodKind::Chili, "Peperoncino", "TURBO", {1.0f, 0.30f, 0.08f}, {1.0f, 0.45f, 0.10f}, 0.55f, 0.46f,
     {0.62f, 1.55f, 0.62f}, MeshShape::Sphere, 20, 1.0f, EffectKind::Turbo, 6.0f, 24.0f, 8.0f},

    // Mirtillo: tempo lento per 5 s (si gira piu' stretto, si va piano).
    {FoodKind::Blueberry, "Mirtillo", "LENTO", {0.38f, 0.48f, 1.0f}, {0.45f, 0.60f, 1.0f}, 0.60f, 0.40f,
     {1.0f, 1.0f, 1.0f}, MeshShape::Sphere, 20, 1.0f, EffectKind::SlowMo, 5.0f, 24.0f, 8.0f},

    // Limone: punti x2 per 10 s.
    {FoodKind::Lemon, "Limone", "x2", {0.96f, 0.94f, 0.30f}, {1.0f, 1.0f, 0.45f}, 0.45f, 0.44f,
     {1.25f, 0.78f, 1.25f}, MeshShape::Sphere, 25, 1.0f, EffectKind::DoubleScore, 10.0f, 22.0f, 7.0f},

    // Fungo velenoso: accorcia di 3 e toglie punti. Forma spinosa = pericolo.
    {FoodKind::Poison, "Fungo velenoso", "VELENO", {0.55f, 0.22f, 0.85f}, {0.75f, 0.30f, 1.0f}, 0.55f, 0.50f,
     {1.0f, 1.0f, 1.0f}, MeshShape::Star, -25, -3.0f, EffectKind::Poisoned, 4.0f, 30.0f, 8.0f},

    // Stella: invincibilita' (si attraversa il proprio corpo e le rocce).
    {FoodKind::Star, "Stella", "STELLA", {1.0f, 0.90f, 0.35f}, {1.0f, 0.95f, 0.55f}, 1.0f, 0.55f,
     {1.0f, 1.0f, 1.0f}, MeshShape::Star, 50, 1.0f, EffectKind::Invincible, 8.0f, 11.0f, 4.0f},

    // Calamita: attira il cibo per 8 s.
    {FoodKind::Magnet, "Calamita", "CALAMITA", {0.95f, 0.30f, 0.28f}, {1.0f, 0.42f, 0.30f}, 0.60f, 0.50f,
     {1.0f, 1.0f, 1.0f}, MeshShape::Torus, 40, 1.0f, EffectKind::Magnet, 8.0f, 17.0f, 4.0f},

    // Frutto bomba: esplode, distrugge rocce e cibo intorno, costa 1 segmento.
    {FoodKind::Bomb, "Frutto bomba", "BOOM", {0.22f, 0.20f, 0.24f}, {1.0f, 0.45f, 0.10f}, 0.75f, 0.50f,
     {1.0f, 1.0f, 1.0f}, MeshShape::Sphere, 40, -1.0f, EffectKind::None, 0.0f, 19.0f, 5.0f},

    // Gemma vita: una vita in piu' (max 5). Rara e con vita breve.
    {FoodKind::Heart, "Gemma vita", "+VITA", {1.0f, 0.35f, 0.60f}, {1.0f, 0.45f, 0.70f}, 0.95f, 0.44f,
     {1.0f, 1.25f, 1.0f}, MeshShape::Octa, 100, 1.0f, EffectKind::None, 0.0f, 10.0f, 2.0f},

    // Cristallo: scudo che assorbe un colpo mortale.
    {FoodKind::Crystal, "Cristallo", "SCUDO", {0.45f, 0.95f, 1.0f}, {0.55f, 1.0f, 1.0f}, 0.85f, 0.50f,
     {1.0f, 1.25f, 1.0f}, MeshShape::Octa, 40, 1.0f, EffectKind::Shield, 12.0f, 15.0f, 3.0f},
}};

}  // namespace

const FoodDef& foodDef(FoodKind kind) {
    const int index = static_cast<int>(kind);
    if (index < 0 || index >= kFoodKindCount) {
        return kFoods[0];
    }
    return kFoods[static_cast<std::size_t>(index)];
}

const FoodDef* foodDefs() { return kFoods.data(); }

const char* effectName(EffectKind effect) {
    switch (effect) {
        case EffectKind::Turbo: return "TURBO";
        case EffectKind::SlowMo: return "TEMPO LENTO";
        case EffectKind::DoubleScore: return "PUNTI x2";
        case EffectKind::Invincible: return "INVINCIBILE";
        case EffectKind::Magnet: return "CALAMITA";
        case EffectKind::Shield: return "SCUDO";
        case EffectKind::Poisoned: return "AVVELENATO";
        case EffectKind::None: break;
    }
    return "";
}

Vec3 effectColor(EffectKind effect) {
    switch (effect) {
        case EffectKind::Turbo: return {1.0f, 0.42f, 0.10f};
        case EffectKind::SlowMo: return {0.45f, 0.62f, 1.0f};
        case EffectKind::DoubleScore: return {1.0f, 0.92f, 0.30f};
        case EffectKind::Invincible: return {1.0f, 0.88f, 0.40f};
        case EffectKind::Magnet: return {1.0f, 0.40f, 0.32f};
        case EffectKind::Shield: return {0.50f, 0.95f, 1.0f};
        case EffectKind::Poisoned: return {0.70f, 0.32f, 1.0f};
        case EffectKind::None: break;
    }
    return {1.0f, 1.0f, 1.0f};
}

}  // namespace w3
