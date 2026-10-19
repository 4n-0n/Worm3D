#include "core/world.hpp"

#include <algorithm>
#include <cmath>

namespace w3 {
namespace {

float segmentRadiusFor(int index, int count) { return Snake::segmentRadius(index, count); }

}  // namespace

void World::reset(uint32_t seed) {
    rng = Rng(seed);
    snake.reset({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 6.0f);
    effects.clearAll();
    foods.clear();
    rocks.clear();
    particles.clear();
    events.clear();

    time = 0.0f;
    score = 0;
    lives = 3;
    level = 1;
    eaten = 0;
    stamina = kStaminaMax;
    grace = 2.0f;
    shake = 0.0f;
    damageFlash = 0.0f;
    poisonPulse = 0.0f;
    boostGlow = 0.0f;
    boosting = false;
    state = RunState::Playing;
    respawnClock_ = 0.0f;
    rockClock_ = 0.0f;

    for (int i = 0; i < 11; ++i) {
        spawnRock();
    }
    for (int i = 0; i < targetFoodCount(); ++i) {
        spawnFood();
    }
}

std::vector<Event> World::takeEvents() {
    std::vector<Event> out;
    out.swap(events);
    return out;
}

float World::speedMultiplier() const {
    float value = effects.speedMultiplier() * (1.0f + 0.035f * static_cast<float>(level - 1));
    if (boosting) {
        value *= 1.5f;
    }
    return std::min(value, 2.6f);
}

int World::targetFoodCount() const { return std::min(64, 34 + level * 2); }

int World::aliveRockCount() const {
    int count = 0;
    for (const Rock& rock : rocks) {
        if (rock.alive) {
            ++count;
        }
    }
    return count;
}

float World::bodyRadius(int index) const {
    return segmentRadiusFor(index, static_cast<int>(snake.body.size()));
}

bool World::collidesWithRock(Vec3 point, float radius) const {
    for (const Rock& rock : rocks) {
        if (!rock.alive) {
            continue;
        }
        // OBB: porta il punto nello spazio locale della roccia (yaw inverso).
        const Vec3 d = point - rock.center;
        const float c = std::cos(rock.yaw);
        const float s = std::sin(rock.yaw);
        const Vec3 local{d.x * c - d.z * s, d.y, d.x * s + d.z * c};
        const Vec3 closest{std::clamp(local.x, -rock.half.x, rock.half.x),
                           std::clamp(local.y, -rock.half.y, rock.half.y),
                           std::clamp(local.z, -rock.half.z, rock.half.z)};
        if (distanceSquared(local, closest) < radius * radius) {
            return true;
        }
    }
    return false;
}

void World::step(float dt, const WorldInput& input) {
    time += dt;
    updateParticles(dt);

    shake = std::max(0.0f, shake - dt * 2.0f);
    damageFlash = std::max(0.0f, damageFlash - dt * 1.6f);
    poisonPulse = std::max(0.0f, poisonPulse - dt * 1.1f);
    boostGlow = std::max(0.0f, boostGlow - dt * 2.4f);
    grace = std::max(0.0f, grace - dt);

    if (state == RunState::GameOver) {
        return;
    }

    effects.advance(dt);

    // --- sterzo: imbardata attorno all'asse verticale del mondo, beccheggio
    // attorno all'asse destro LOCALE. Il beccheggio non ha limiti: il verme può
    // compiere giri completi. dir e up ruotano insieme, altrimenti a 90° l'asse
    // di rotazione (cross(up, dir)) degenera e il muso si blocca.
    const float turnRate = 3.0f * effects.turnMultiplier();
    const Vec3 worldUp{0.0f, 1.0f, 0.0f};

    const float yawAngle = input.yaw * turnRate * dt;
    if (std::abs(yawAngle) > 1e-7f) {
        snake.dir = rotateAroundAxis(snake.dir, worldUp, yawAngle);
        snake.up = rotateAroundAxis(snake.up, worldUp, yawAngle);
    }

    Vec3 right = cross(snake.up, snake.dir);
    if (lengthSquared(right) < 1e-6f) {
        right = cross(worldUp, snake.dir);
    }
    right = normalize(right);

    const float pitchAngle = input.pitch * turnRate * dt;
    if (std::abs(pitchAngle) > 1e-7f) {
        snake.dir = rotateAroundAxis(snake.dir, right, -pitchAngle);
        snake.up = rotateAroundAxis(snake.up, right, -pitchAngle);
    }

    snake.dir = normalize(snake.dir);
    if (lengthSquared(snake.dir) < 1e-6f) {
        snake.dir = {0.0f, 0.0f, 1.0f};
        snake.up = {0.0f, 1.0f, 0.0f};
    }
    right = cross(snake.up, snake.dir);
    if (lengthSquared(right) < 1e-6f) {
        right = cross(worldUp, snake.dir);
    }
    right = normalize(right);
    // dir × right = up (l'ordine opposto restituirebbe -up e invertirebbe il beccheggio).
    snake.up = normalize(cross(snake.dir, right));

    // --- scatto con stamina ---
    const bool wantsBoost = input.boost && stamina > 0.04f;
    boosting = wantsBoost;
    if (wantsBoost) {
        stamina = std::max(0.0f, stamina - dt);
        boostGlow = 1.0f;
    } else {
        stamina = std::min(kStaminaMax, stamina + dt * 0.55f);
    }

    Vec3 next = snake.head + snake.dir * (kBaseSpeed * speedMultiplier() * dt);

    // --- pareti: rimbalzo (in 3D la morte sul muro sarebbe troppo punitiva) ---
    bool bounced = false;
    const float limitX = kHalf.x - Snake::kHeadRadius;
    const float limitY = kHalf.y - Snake::kHeadRadius;
    const float limitZ = kHalf.z - Snake::kHeadRadius;
    if (next.x > limitX) {
        next.x = limitX;
        snake.dir.x = -std::abs(snake.dir.x);
        bounced = true;
    } else if (next.x < -limitX) {
        next.x = -limitX;
        snake.dir.x = std::abs(snake.dir.x);
        bounced = true;
    }
    if (next.y > limitY) {
        next.y = limitY;
        snake.dir.y = -std::abs(snake.dir.y);
        bounced = true;
    } else if (next.y < -limitY) {
        next.y = -limitY;
        snake.dir.y = std::abs(snake.dir.y);
        bounced = true;
    }
    if (next.z > limitZ) {
        next.z = limitZ;
        snake.dir.z = -std::abs(snake.dir.z);
        bounced = true;
    } else if (next.z < -limitZ) {
        next.z = -limitZ;
        snake.dir.z = std::abs(snake.dir.z);
        bounced = true;
    }
    if (bounced) {
        snake.dir = normalize(snake.dir);
        score = std::max(0, score - 2);
        shake = std::max(shake, 0.5f);
        emitBurst(snake.head, {0.40f, 0.85f, 1.0f}, 8, 3.0f, 0.16f);
        events.push_back({EventKind::Bounce, snake.head, FoodKind::Apple, EffectKind::None, 0});
    }

    snake.pushHead(next);
    snake.rebuildBody();

    // --- calamita: il cibo viene risucchiato verso la testa ---
    if (effects.has(EffectKind::Magnet)) {
        for (Food& food : foods) {
            if (!food.alive) {
                continue;
            }
            const float d = distance(food.position, snake.head);
            if (d > 0.6f && d < 9.0f) {
                food.position += normalize(snake.head - food.position) * (std::min(9.0f, d * 4.0f) * dt);
            }
        }
    }

    // --- raccolta del cibo ---
    const Vec3 head = snake.head;
    for (Food& food : foods) {
        if (!food.alive) {
            continue;
        }
        food.age += dt;
        if (food.lifespan > 0.0f && food.age >= food.lifespan) {
            food.alive = false;
            continue;
        }
        const FoodDef& def = foodDef(food.kind);
        const float pickup = def.radius + Snake::kHeadRadius * 0.95f + 0.15f;
        if (distanceSquared(food.position, head) <= pickup * pickup) {
            applyFood(food);
        }
    }

    // --- spawn del cibo: uno ogni 80 ms finche' l'arena non e' piena ---
    respawnClock_ += dt;
    if (respawnClock_ >= 0.08f) {
        respawnClock_ = 0.0f;
        const int alive = static_cast<int>(std::count_if(
            foods.begin(), foods.end(), [](const Food& f) { return f.alive; }));
        if (alive < targetFoodCount()) {
            spawnFood();
        }
    }

    // --- le rocce ricrescono lentamente per non fossilizzare l'arena ---
    rockClock_ += dt;
    if (rockClock_ > 18.0f) {
        rockClock_ = 0.0f;
        if (aliveRockCount() < 8 + level) {
            spawnRock();
        }
    }

    // --- collisioni letali (stella = si attraversa tutto) ---
    if (grace <= 0.0f && !effects.has(EffectKind::Invincible)) {
        bool lethal = false;
        const int count = static_cast<int>(snake.body.size());
        for (int i = 6; i < count; ++i) {
            const float radius = bodyRadius(i) * 0.92f + Snake::kHeadRadius * 0.72f;
            if (distanceSquared(snake.body[static_cast<std::size_t>(i)], snake.head) < radius * radius) {
                lethal = true;
                break;
            }
        }
        if (!lethal && collidesWithRock(snake.head, Snake::kHeadRadius * 0.90f)) {
            lethal = true;
        }
        if (lethal) {
            die();
        }
    }

    foods.erase(std::remove_if(foods.begin(), foods.end(), [](const Food& f) { return !f.alive; }),
                foods.end());
    rocks.erase(std::remove_if(rocks.begin(), rocks.end(), [](const Rock& r) { return !r.alive; }),
                rocks.end());
}

void World::applyFood(Food& food) {
    food.alive = false;
    const FoodDef& def = foodDef(food.kind);

    int gain = def.score;
    if (gain > 0) {
        gain = static_cast<int>(std::lround(static_cast<float>(gain) * effects.scoreMultiplier()));
    }
    score = std::max(0, score + gain);
    snake.length = std::clamp(snake.length + def.growth, Snake::kMinLength, Snake::kMaxLength);

    events.push_back({EventKind::Eat, food.position, food.kind, def.effect, gain});
    emitBurst(food.position, def.glow, 18, 4.5f, 0.24f);

    if (def.effect != EffectKind::None) {
        effects.apply(def.effect, def.effectSeconds);
        events.push_back({EventKind::Power, food.position, food.kind, def.effect,
                          static_cast<int>(std::lround(def.effectSeconds))});
    }
    if (def.effect == EffectKind::Poisoned) {
        poisonPulse = 1.0f;
        damageFlash = 0.55f;
        shake = std::max(shake, 0.6f);
    }

    switch (food.kind) {
        case FoodKind::Heart:
            lives = std::min(5, lives + 1);
            events.push_back({EventKind::ExtraLife, food.position, food.kind, EffectKind::None, lives});
            break;
        case FoodKind::Bomb:
            explode(food.position);
            break;
        default:
            break;
    }

    if (def.growth > 0.0f) {
        ++eaten;
        const int newLevel = 1 + eaten / 8;
        if (newLevel > level) {
            level = newLevel;
            events.push_back({EventKind::LevelUp, food.position, food.kind, EffectKind::None, level});
        }
    }
}

void World::explode(Vec3 center) {
    shake = std::max(shake, 1.3f);
    damageFlash = std::max(damageFlash, 0.35f);
    events.push_back({EventKind::Explosion, center, FoodKind::Bomb, EffectKind::None, 0});
    emitBurst(center, {1.0f, 0.55f, 0.15f}, 80, 10.0f, 0.36f);
    emitRing(center, {1.0f, 0.32f, 0.10f}, 46, 13.0f, 0.28f);

    for (Food& food : foods) {
        if (!food.alive || distance(food.position, center) >= 5.5f) {
            continue;
        }
        food.alive = false;
        score += 5;
        emitBurst(food.position, foodDef(food.kind).glow, 6, 3.0f, 0.20f);
    }
    for (Rock& rock : rocks) {
        if (!rock.alive || distance(rock.center, center) >= 5.5f + length(rock.half)) {
            continue;
        }
        rock.alive = false;
        score += 15;
        emitBurst(rock.center, {0.60f, 0.72f, 0.82f}, 30, 6.0f, 0.30f);
        events.push_back({EventKind::RockBreak, rock.center, FoodKind::Bomb, EffectKind::None, 15});
    }
}

void World::die() {
    if (effects.has(EffectKind::Shield)) {
        // Lo scudo assorbe il colpo: si resta vivi e invulnerabili per uscire.
        effects.clear(EffectKind::Shield);
        grace = 1.8f;
        shake = std::max(shake, 1.0f);
        damageFlash = 0.6f;
        emitBurst(snake.head, {0.50f, 0.95f, 1.0f}, 54, 7.0f, 0.30f);
        events.push_back({EventKind::ShieldBreak, snake.head, FoodKind::Crystal, EffectKind::Shield, 0});
        return;
    }

    --lives;
    emitBurst(snake.head, {1.0f, 0.35f, 0.20f}, 110, 8.0f, 0.42f);
    shake = 1.5f;
    damageFlash = 1.0f;
    events.push_back({EventKind::Death, snake.head, FoodKind::Apple, EffectKind::None, lives});

    if (lives <= 0) {
        state = RunState::GameOver;
        events.push_back({EventKind::GameOver, snake.head, FoodKind::Apple, EffectKind::None, score});
        return;
    }
    respawn();
}

void World::respawn() {
    snake.reset({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 6.0f);
    effects.clearAll();
    stamina = kStaminaMax;
    grace = 3.0f;
    // Niente cibo regalato sulla testa appena rinata.
    for (Food& food : foods) {
        if (!food.alive) {
            continue;
        }
        if (distance(food.position, {0.0f, 0.0f, 0.0f}) < 3.5f) {
            food.alive = false;
        }
    }
    events.push_back({EventKind::Respawn, snake.head, FoodKind::Apple, EffectKind::None, lives});
}

void World::spawnFood() {
    float total = 0.0f;
    const FoodDef* defs = foodDefs();
    for (int i = 0; i < kFoodKindCount; ++i) {
        total += defs[i].weight;
    }

    float roll = rng.range(0.0f, total);
    FoodKind kind = FoodKind::Apple;
    for (int i = 0; i < kFoodKindCount; ++i) {
        roll -= defs[i].weight;
        if (roll <= 0.0f) {
            kind = defs[i].kind;
            break;
        }
    }

    for (int attempt = 0; attempt < 50; ++attempt) {
        const Vec3 point = randomArenaPoint(1.5f);
        if (!spawnPointFree(point)) {
            continue;
        }
        Food food;
        food.kind = kind;
        food.position = point;
        food.lifespan = foodDef(kind).lifespan;
        food.phase = rng.range(0.0f, 6.2831853f);
        food.spin = rng.range(-1.5f, 1.5f);
        foods.push_back(food);
        return;
    }
}

void World::spawnRock() {
    for (int attempt = 0; attempt < 60; ++attempt) {
        const Vec3 center = randomArenaPoint(3.0f);
        // Zona di partenza libera: la prima inquadratura non deve nascere
        // dentro o dietro un ostacolo.
        if (length(center) < 13.0f) {
            continue;
        }
        const Vec3 half{rng.range(0.9f, 2.3f), rng.range(0.7f, 1.8f), rng.range(0.9f, 2.3f)};
        bool clash = false;
        for (const Rock& rock : rocks) {
            if (!rock.alive) {
                continue;
            }
            if (distance(center, rock.center) < length(rock.half) + length(half) + 1.0f) {
                clash = true;
                break;
            }
        }
        if (clash || distance(center, snake.head) < 4.0f) {
            continue;
        }
        Rock rock;
        rock.center = center;
        rock.half = half;
        rock.yaw = rng.range(0.0f, 3.14159265f);
        rocks.push_back(rock);
        return;
    }
}

Vec3 World::randomArenaPoint(float inset) {
    return {rng.range(-kHalf.x + inset, kHalf.x - inset),
            rng.range(-kHalf.y + inset, kHalf.y - inset),
            rng.range(-kHalf.z + inset, kHalf.z - inset)};
}

bool World::spawnPointFree(Vec3 point) const {
    if (std::abs(point.x) > kHalf.x - 1.0f || std::abs(point.y) > kHalf.y - 1.0f ||
        std::abs(point.z) > kHalf.z - 1.0f) {
        return false;
    }
    if (distanceSquared(point, snake.head) < 3.5f * 3.5f) {
        return false;
    }
    if (collidesWithRock(point, 0.9f)) {
        return false;
    }
    for (const Food& food : foods) {
        if (food.alive && distanceSquared(point, food.position) < 1.6f * 1.6f) {
            return false;
        }
    }
    return true;
}

void World::emitBurst(Vec3 at, Vec3 color, int count, float speed, float size) {
    for (int i = 0; i < count; ++i) {
        if (particles.size() >= kMaxParticles) {
            return;
        }
        Vec3 dir{rng.range(-1.0f, 1.0f), rng.range(-1.0f, 1.0f), rng.range(-1.0f, 1.0f)};
        dir = normalize(dir);
        if (lengthSquared(dir) < 1e-6f) {
            dir = {0.0f, 1.0f, 0.0f};
        }
        Particle particle;
        particle.position = at + dir * rng.range(0.0f, 0.30f);
        particle.velocity = dir * (speed * rng.range(0.25f, 1.0f));
        particle.color = color * rng.range(0.70f, 1.15f);
        particle.maxLife = rng.range(0.35f, 0.95f);
        particle.life = particle.maxLife;
        particle.size = size * rng.range(0.6f, 1.5f);
        particles.push_back(particle);
    }
}

void World::emitRing(Vec3 at, Vec3 color, int count, float speed, float size) {
    for (int i = 0; i < count; ++i) {
        if (particles.size() >= kMaxParticles) {
            return;
        }
        const float angle = (static_cast<float>(i) / static_cast<float>(count)) * 6.2831853f +
                            rng.range(-0.05f, 0.05f);
        const Vec3 dir{std::cos(angle), rng.range(-0.18f, 0.18f), std::sin(angle)};
        Particle particle;
        particle.position = at + dir * 0.25f;
        particle.velocity = dir * (speed * rng.range(0.8f, 1.1f));
        particle.color = color * rng.range(0.85f, 1.2f);
        particle.maxLife = rng.range(0.30f, 0.60f);
        particle.life = particle.maxLife;
        particle.size = size * rng.range(0.8f, 1.4f);
        particles.push_back(particle);
    }
}

void World::updateParticles(float dt) {
    for (Particle& particle : particles) {
        particle.life -= dt;
        particle.velocity *= (1.0f - std::min(0.9f, 1.8f * dt));
        particle.velocity.y -= 2.2f * dt;
        particle.position += particle.velocity * dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
                                   [](const Particle& p) { return p.life <= 0.0f; }),
                    particles.end());
}

}  // namespace w3
