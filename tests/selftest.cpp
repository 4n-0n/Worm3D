// selftest.cpp — test della simulazione (nessun Qt, nessun display).
//
// Ogni test costruisce un mondo controllato: si svuotano rocce e cibo per
// isolare la regola sotto esame, poi si avanza a passo fisso.

#include "core/autopilot.hpp"
#include "core/world.hpp"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

#define CHECK(condition)                                                              \
    do {                                                                              \
        if (!(condition)) {                                                           \
            std::printf("  FALLITO %s:%d  %s\n", __FILE__, __LINE__, #condition);     \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

constexpr float kStep = 1.0f / 120.0f;

void clearArena(w3::World& world) {
    world.foods.clear();
    world.rocks.clear();
    world.particles.clear();
    world.grace = 0.0f;
}

void advance(w3::World& world, int steps, w3::WorldInput input = {}) {
    for (int i = 0; i < steps; ++i) {
        world.step(kStep, input);
    }
}

void testFoodTable() {
    std::printf("tabella cibo\n");
    const w3::FoodDef* defs = w3::foodDefs();
    for (int i = 0; i < w3::kFoodKindCount; ++i) {
        CHECK(static_cast<int>(defs[i].kind) == i);
        CHECK(defs[i].radius > 0.1f);
        CHECK(defs[i].weight > 0.0f);
        CHECK(defs[i].name != nullptr);
        CHECK(defs[i].renderScale.x > 0.0f);
    }
    // Almeno un cibo per ogni effetto: il catalogo deve coprire tutti gli stati.
    for (int e = 0; e < w3::kEffectCount; ++e) {
        bool found = false;
        for (int i = 0; i < w3::kFoodKindCount; ++i) {
            if (static_cast<int>(defs[i].effect) == e) {
                found = true;
            }
        }
        CHECK(found);
    }
}

void testEatGrows() {
    std::printf("mela: +1 segmento e +10 punti\n");
    w3::World world;
    world.reset(7);
    clearArena(world);
    const float before = world.snake.length;

    w3::Food apple;
    apple.kind = w3::FoodKind::Apple;
    apple.position = world.snake.head + world.snake.dir * 1.0f;
    world.foods.push_back(apple);

    advance(world, 1);
    CHECK(world.snake.length == before + 1.0f);
    CHECK(world.score == 10);
    CHECK(world.snake.segmentCount() == 7);
    CHECK(world.foods.empty());
}

void testPoisonShrinks() {
    std::printf("veleno: -3 segmenti\n");
    w3::World world;
    world.reset(8);
    clearArena(world);
    world.snake.length = 10.0f;

    w3::Food poison;
    poison.kind = w3::FoodKind::Poison;
    poison.position = world.snake.head + world.snake.dir * 1.0f;
    world.foods.push_back(poison);

    advance(world, 1);
    CHECK(world.snake.length == 7.0f);
    CHECK(world.effects.has(w3::EffectKind::Poisoned));
    CHECK(world.score == 0);  // il punteggio non scende sotto zero
}

void testTurboFaster() {
    std::printf("peperoncino: corsa piu' veloce\n");
    w3::World normal;
    w3::World turbo;
    normal.reset(9);
    turbo.reset(9);
    clearArena(normal);
    clearArena(turbo);
    turbo.effects.apply(w3::EffectKind::Turbo, 6.0f);

    const w3::Vec3 start = normal.snake.head;
    advance(normal, 60);
    advance(turbo, 60);
    const float normalDistance = w3::distance(start, normal.snake.head);
    const float turboDistance = w3::distance(start, turbo.snake.head);
    CHECK(turboDistance > normalDistance * 1.4f);
}

void testWallBounce() {
    std::printf("parete: rimbalzo invece di morte\n");
    w3::World world;
    world.reset(10);
    clearArena(world);
    world.snake.reset({15.5f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 6.0f);

    advance(world, 1);
    CHECK(world.lives == 3);
    CHECK(world.snake.dir.x < 0.0f);
    CHECK(world.snake.head.x <= w3::World::kHalf.x - w3::Snake::kHeadRadius + 0.01f);
}

void testShieldAndDeath() {
    std::printf("cristallo assorbe un colpo, poi si muore\n");
    w3::World world;
    world.reset(11);
    clearArena(world);
    world.effects.apply(w3::EffectKind::Shield, 12.0f);

    w3::Rock rock;
    rock.center = {0.0f, 0.0f, 1.1f};
    rock.half = {1.0f, 1.0f, 1.0f};
    world.rocks.push_back(rock);

    advance(world, 1);
    CHECK(world.lives == 3);
    CHECK(!world.effects.has(w3::EffectKind::Shield));
    CHECK(world.grace > 0.0f);

    world.rocks.clear();
    world.grace = 0.0f;
    world.snake.reset({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 6.0f);
    world.rocks.push_back(rock);
    advance(world, 1);
    CHECK(world.lives == 2);
}

void testExplosion() {
    std::printf("bomba: distrugge cibo e rocce vicine\n");
    w3::World world;
    world.reset(12);
    clearArena(world);
    world.effects.clearAll();

    w3::Food bomb;
    bomb.kind = w3::FoodKind::Bomb;
    bomb.position = {0.0f, 0.0f, 1.0f};
    world.foods.push_back(bomb);
    for (float z : {2.0f, 3.0f}) {
        w3::Food apple;
        apple.kind = w3::FoodKind::Apple;
        apple.position = {0.0f, 0.0f, z};
        world.foods.push_back(apple);
    }
    w3::Rock rock;
    rock.center = {0.0f, 0.0f, 2.5f};
    rock.half = {0.8f, 0.8f, 0.8f};
    world.rocks.push_back(rock);

    advance(world, 1);
    CHECK(world.foods.empty());
    CHECK(world.rocks.empty());
    CHECK(world.score >= 65);
}

void testDeterminism() {
    std::printf("determinismo: stesso seed, stessa partita\n");
    w3::World a;
    w3::World b;
    a.reset(42);
    b.reset(42);
    for (int i = 0; i < 600; ++i) {
        const w3::WorldInput input = w3::autopilotInput(a);
        a.step(kStep, input);
        b.step(kStep, input);
    }
    CHECK(a.score == b.score);
    CHECK(a.lives == b.lives);
    CHECK(a.foods.size() == b.foods.size());
    CHECK(a.particles.size() == b.particles.size());
    CHECK(a.snake.head.x == b.snake.head.x);
    CHECK(a.snake.head.y == b.snake.head.y);
    CHECK(a.snake.head.z == b.snake.head.z);
}

void testAutopilotPlays() {
    std::printf("autopilota: mangia e resta vivo\n");
    w3::World world;
    world.reset(5);
    for (int i = 0; i < 1800; ++i) {
        world.step(kStep, w3::autopilotInput(world));
    }
    CHECK(world.score > 0);
    CHECK(world.lives >= 1);
    CHECK(world.snake.length > 6.0f);
    CHECK(world.snake.head.y > -w3::World::kHalf.y);
}

void testBodyGeometry() {
    std::printf("corpo: campioni equidistanti dalla testa\n");
    w3::World world;
    world.reset(13);
    clearArena(world);
    world.snake.length = 12.0f;
    advance(world, 240, {0.35f, 0.1f, false});

    CHECK(static_cast<int>(world.snake.body.size()) == world.snake.segmentCount());
    CHECK(w3::distance(world.snake.body.front(), world.snake.head) < 0.05f);
    for (std::size_t i = 1; i < world.snake.body.size(); ++i) {
        const float expected = static_cast<float>(i) * w3::Snake::kSpacing;
        const float actual = w3::distance(world.snake.head, world.snake.body[i]);
        CHECK(std::abs(actual - expected) < 0.65f);
    }
}

void testSteeringAxes() {
    std::printf("assi di sterzo: +yaw verso +X, +pitch verso +Y\n");
    w3::World world;
    world.reset(21);
    clearArena(world);
    world.snake.reset({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 6.0f);
    advance(world, 30, {1.0f, 0.0f, false});
    CHECK(world.snake.dir.x > 0.1f);
    CHECK(std::abs(world.snake.dir.y) < 0.1f);

    world.snake.reset({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 6.0f);
    advance(world, 30, {0.0f, 1.0f, false});
    CHECK(world.snake.dir.y > 0.1f);
}

void testFullPitchRotation() {
    std::printf("beccheggio: giro completo, nessun limite a 180 gradi\n");
    w3::World world;
    world.reset(31);
    clearArena(world);
    world.snake.reset({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 3.0f);
    world.grace = 1000.0f;  // isola la rotazione dall'autocollisione

    bool wentUp = false;
    bool flipped = false;
    for (int i = 0; i < 300; ++i) {
        world.step(kStep, {0.0f, 1.0f, false});  // S: muso in alto, sempre
        if (world.snake.dir.y > 0.90f) {
            wentUp = true;
        }
        if (world.snake.dir.y < -0.50f) {
            flipped = true;
        }
    }
    CHECK(wentUp);
    CHECK(flipped);
    CHECK(world.lives == 3);
    // La base deve restare ortogonale anche capovolti.
    CHECK(std::abs(w3::dot(world.snake.dir, world.snake.up)) < 1e-3f);
}

void testLevelProgress() {
    std::printf("livello: sale ogni 8 cibi\n");
    w3::World world;
    world.reset(14);
    clearArena(world);
    world.snake.length = 60.0f;
    for (int i = 0; i < 8; ++i) {
        w3::Food apple;
        apple.kind = w3::FoodKind::Apple;
        apple.position = world.snake.head + world.snake.dir * 1.0f;
        world.foods.push_back(apple);
        advance(world, 1);
        world.foods.clear();
    }
    CHECK(world.eaten == 8);
    CHECK(world.level == 2);
}

}  // namespace

int main() {
    testFoodTable();
    testEatGrows();
    testPoisonShrinks();
    testTurboFaster();
    testWallBounce();
    testShieldAndDeath();
    testExplosion();
    testDeterminism();
    testAutopilotPlays();
    testBodyGeometry();
    testSteeringAxes();
    testFullPitchRotation();
    testLevelProgress();

    if (g_failures == 0) {
        std::printf("\nTUTTI I TEST PASSANO\n");
        return 0;
    }
    std::printf("\n%d TEST FALLITI\n", g_failures);
    return 1;
}
