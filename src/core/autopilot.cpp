#include "core/autopilot.hpp"

#include <algorithm>
#include <cmath>

namespace w3 {

WorldInput autopilotInput(const World& world) {
    WorldInput input;
    const Vec3 head = world.snake.head;
    const Vec3 dir = world.snake.dir;
    const Vec3 up{0.0f, 1.0f, 0.0f};

    Vec3 right = cross(up, dir);
    if (lengthSquared(right) < 1e-4f) {
        right = cross(world.snake.up, dir);
    }
    right = normalize(right);
    if (lengthSquared(right) < 1e-6f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    Vec3 upPerp = cross(dir, right);
    if (lengthSquared(upPerp) < 1e-6f) {
        upPerp = cross(dir, Vec3{1.0f, 0.0f, 0.0f});
    }
    upPerp = normalize(upPerp);

    // Bersaglio: cibo commestibile, preferendo quello davanti. Senza questa
    // preferenza il pilota orbita attorno al cibo senza mai raccoglierlo.
    Vec3 target{0.0f, 0.0f, 0.0f};
    float bestScore = 1e9f;
    for (const Food& food : world.foods) {
        if (!food.alive || foodDef(food.kind).score < 0) {
            continue;
        }
        const Vec3 offset = food.position - head;
        const float d = length(offset);
        const float facing = d > 1e-4f ? dot(offset / d, dir) : 1.0f;
        const float score = d - 3.0f * facing;
        if (score < bestScore) {
            bestScore = score;
            target = food.position;
        }
    }

    // Vicino al bordo si punta al centro: i rimbalzi disorientano.
    const float margin = 2.5f;
    if (std::abs(head.x) > World::kHalf.x - margin || std::abs(head.y) > World::kHalf.y - margin ||
        std::abs(head.z) > World::kHalf.z - margin) {
        target = {0.0f, 0.0f, 0.0f};
    }

    Vec3 desired = normalize(target - head);
    if (lengthSquared(desired) < 1e-6f) {
        desired = dir;
    }

    // Rocce davanti: si alza il muso invece di schiantarsi.
    bool rockAhead = false;
    for (float ahead = 1.2f; ahead <= 5.0f; ahead += 1.2f) {
        if (world.collidesWithRock(head + dir * ahead, 1.1f)) {
            rockAhead = true;
            break;
        }
    }
    if (rockAhead) {
        desired = normalize(dir + upPerp * 2.2f + right * 0.5f);
    } else {
        // Autocollisione: se un punto davanti e' vicino alla propria coda,
        // il pilota vira per non mordersi.
        const Vec3 aheadPoint = head + dir * 2.6f;
        Vec3 away{0.0f, 0.0f, 0.0f};
        for (std::size_t i = 12; i < world.snake.body.size(); i += 3) {
            const Vec3 offset = aheadPoint - world.snake.body[i];
            const float distanceSq = lengthSquared(offset);
            if (distanceSq < 2.25f && distanceSq > 1e-4f) {
                away += normalize(offset);
            }
        }
        if (lengthSquared(away) > 1e-4f) {
            desired = normalize(desired + normalize(away) * 1.6f + upPerp * 0.5f);
        }
    }

    const float yawError = std::atan2(dot(desired, right), dot(desired, dir));
    const float pitchError = std::asin(std::clamp(dot(desired, upPerp), -1.0f, 1.0f));
    input.yaw = std::clamp(yawError * 2.2f, -1.0f, 1.0f);
    // Il pilota non chiede mai giri completi: con un verme lungo finirebbe
    // per mordersi la coda.
    input.pitch = std::clamp(pitchError * 2.6f, -0.75f, 0.75f);
    input.boost = std::abs(yawError) < 0.30f && std::abs(pitchError) < 0.30f &&
                  distance(target, head) > 4.0f;
    return input;
}

}  // namespace w3
