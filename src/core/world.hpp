#pragma once
// world.hpp — tutta la simulazione. Puro C++: niente Qt, niente GL, nessun I/O.
//
// Contratti:
//   - step(dt) e' deterministico dato il seed: usato da test e da --capture.
//   - il mondo non conosce il rendering; emette Event per audio/HUD/particelle.
//   - le uniche mutazioni di stato avvengono in reset() e step().

#include "core/effects.hpp"
#include "core/food.hpp"
#include "core/rng.hpp"
#include "core/snake.hpp"

#include <vector>

namespace w3 {

enum class RunState {
    Playing,
    GameOver,
};

enum class EventKind {
    Eat,          // mangiato un cibo (food, amount = punti)
    Power,        // attivato un effetto (effect)
    Bounce,       // rimbalzo sul bordo dell'arena
    Death,        // colpo mortale
    GameOver,
    Respawn,
    Explosion,    // frutto bomba
    RockBreak,
    ExtraLife,
    ShieldBreak,  // lo scudo ha assorbito un colpo
    LevelUp,
};

struct Event {
    EventKind kind = EventKind::Eat;
    Vec3 position;
    FoodKind food = FoodKind::Apple;
    EffectKind effect = EffectKind::None;
    int amount = 0;
};

struct Food {
    FoodKind kind = FoodKind::Apple;
    Vec3 position;
    float age = 0.0f;
    float lifespan = 0.0f;  // <= 0: non scade
    float phase = 0.0f;     // sfasamento dell'animazione
    float spin = 0.0f;
    bool alive = true;
};

struct Rock {
    Vec3 center;
    Vec3 half;
    float yaw = 0.0f;
    bool alive = true;
};

struct Particle {
    Vec3 position;
    Vec3 velocity;
    Vec3 color;
    float life = 0.0f;
    float maxLife = 1.0f;
    float size = 0.2f;
};

struct WorldInput {
    float yaw = 0.0f;    // +1 = verso destra dello schermo
    float pitch = 0.0f;  // +1 = verso l'alto
    bool boost = false;
};

struct World {
    static constexpr Vec3 kHalf{16.0f, 9.0f, 16.0f};
    static constexpr float kBaseSpeed = 6.2f;
    static constexpr float kStaminaMax = 3.2f;
    static constexpr std::size_t kMaxParticles = 1100;

    Snake snake;
    Effects effects;
    std::vector<Food> foods;
    std::vector<Rock> rocks;
    std::vector<Particle> particles;
    std::vector<Event> events;
    Rng rng{1u};

    float time = 0.0f;
    int score = 0;
    int lives = 3;
    int level = 1;
    int eaten = 0;
    float stamina = kStaminaMax;
    float grace = 0.0f;        // invulnerabilita' dopo un risveglio
    float shake = 0.0f;        // ampiezza camera shake, decade
    float damageFlash = 0.0f;  // flash rosso a schermo
    float poisonPulse = 0.0f;  // tinta viola a schermo
    float boostGlow = 0.0f;    // 0..1 per l'HUD
    bool boosting = false;
    RunState state = RunState::Playing;

    void reset(uint32_t seed = 1u);
    void step(float dt, const WorldInput& input);

    std::vector<Event> takeEvents();
    float speedMultiplier() const;
    int targetFoodCount() const;
    int aliveRockCount() const;
    bool collidesWithRock(Vec3 point, float radius) const;

    // Utile al renderer: il raggio di un segmento del corpo.
    float bodyRadius(int index) const;

private:
    void applyFood(Food& food);
    void explode(Vec3 center);
    void die();
    void respawn();
    void spawnFood();
    void spawnRock();
    void updateParticles(float dt);
    void emitBurst(Vec3 at, Vec3 color, int count, float speed, float size);
    void emitRing(Vec3 at, Vec3 color, int count, float speed, float size);
    Vec3 randomArenaPoint(float inset);
    bool spawnPointFree(Vec3 point) const;

    float respawnClock_ = 0.0f;
    float rockClock_ = 0.0f;
};

}  // namespace w3
