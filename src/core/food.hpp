#pragma once
// food.hpp — catalogo del cibo. Una sola tabella descrive TUTTO:
// aspetto (forma, colore, alone), punteggio, crescita e effetto temporaneo.
//
// Aggiungere un cibo = aggiungere una riga in food.cpp + un valore in FoodKind.
// Il renderer e l'HUD leggono la tabella, non hanno switch duplicati.

#include "core/vec3.hpp"

namespace w3 {

enum class FoodKind : int {
    Apple = 0,
    GoldenApple,
    Chili,
    Blueberry,
    Lemon,
    Poison,
    Star,
    Magnet,
    Bomb,
    Heart,
    Crystal,
};

inline constexpr int kFoodKindCount = 11;

// Forme disponibili nel renderer (mesh dedicate, disegnate in istanze).
enum class MeshShape : int {
    Sphere = 0,
    Cube,
    Star,
    Octa,
    Torus,
    Count,
};

enum class EffectKind : int {
    None = -1,
    Turbo = 0,
    SlowMo,
    DoubleScore,
    Invincible,
    Magnet,
    Shield,
    Poisoned,
};

inline constexpr int kEffectCount = 7;

struct FoodDef {
    FoodKind kind;
    const char* name;   // nome esteso (HUD / messaggi)
    const char* shortName;
    Vec3 color;         // colore del corpo
    Vec3 glow;          // colore dell'alone additivo
    float emissive;     // 0..1: quanto il corpo si illumina da solo
    float radius;       // raggio di collisione e dimensione base
    Vec3 renderScale;   // deformazione della mesh (peperoncino = ellissoide)
    MeshShape shape;
    int score;
    float growth;        // segmenti guadagnati (negativo = accorcia)
    EffectKind effect;
    float effectSeconds;
    float lifespan;      // secondi prima di sparire; <= 0 = eterno
    float weight;        // peso nella scelta casuale dello spawn
};

const FoodDef& foodDef(FoodKind kind);
const FoodDef* foodDefs();
const char* effectName(EffectKind effect);
Vec3 effectColor(EffectKind effect);

}  // namespace w3
