#include "render/renderer.hpp"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace w3 {
namespace {

constexpr Vec3 kFogColor{0.018f, 0.075f, 0.095f};
constexpr float kFogDensity = 0.0095f;

Vec3 tintForEffects(const World& world, Vec3 color, float time) {
    if (world.effects.has(EffectKind::Poisoned)) {
        return lerp(color, Vec3{0.45f, 0.18f, 0.85f}, 0.60f);
    }
    if (world.effects.has(EffectKind::Invincible)) {
        const float pulse = 0.55f + 0.15f * std::sin(time * 12.0f);
        return lerp(color, Vec3{1.0f, 0.86f, 0.35f}, pulse);
    }
    if (world.effects.has(EffectKind::Shield)) {
        return lerp(color, Vec3{0.35f, 0.92f, 1.0f}, 0.40f);
    }
    if (world.effects.has(EffectKind::Turbo)) {
        return lerp(color, Vec3{1.0f, 0.45f, 0.12f}, 0.35f);
    }
    return color;
}

QVector3D toQt(Vec3 v) { return QVector3D(v.x, v.y, v.z); }

}  // namespace

bool Renderer::init(QOpenGLExtraFunctions* gl, QString* error) {
    if (!sceneProg_.build(gl, ":/shaders/scene.vert", ":/shaders/scene.frag", error)) return false;
    if (!gridProg_.build(gl, ":/shaders/grid.vert", ":/shaders/grid.frag", error)) return false;
    if (!skyProg_.build(gl, ":/shaders/sky.vert", ":/shaders/sky.frag", error)) return false;
    if (!glowProg_.build(gl, ":/shaders/glow.vert", ":/shaders/glow.frag", error)) return false;
    if (!lineProg_.build(gl, ":/shaders/line.vert", ":/shaders/line.frag", error)) return false;

    for (int i = 0; i < kShapeCount; ++i) {
        const MeshData data = makeMesh(static_cast<MeshShape>(i));
        std::vector<glx::Vertex> vertices;
        vertices.reserve(data.vertices.size());
        for (const MeshVertex& v : data.vertices) {
            vertices.push_back({v.px, v.py, v.pz, v.nx, v.ny, v.nz});
        }
        meshes_[i] = glx::upload(gl, vertices, data.indices);
        if (meshes_[i].vao == 0) {
            if (error) *error = QStringLiteral("mesh %1 vuota").arg(i);
            return false;
        }
    }

    gl->glGenBuffers(1, &instanceVbo_);
    gl->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    gl->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    for (int i = 0; i < kShapeCount; ++i) {
        setupInstanceAttributes(gl, meshes_[i].vao);
    }

    // Quad per griglia e aloni.
    const float quadCorners[8] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    gl->glGenVertexArrays(1, &quadVao_);
    gl->glBindVertexArray(quadVao_);
    gl->glGenBuffers(1, &quadVbo_);
    gl->glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(quadCorners), quadCorners, GL_STATIC_DRAW);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    gl->glBindVertexArray(0);

    // Aloni: stesso quad + attributi istanziati.
    gl->glGenVertexArrays(1, &glowVao_);
    gl->glBindVertexArray(glowVao_);
    gl->glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    gl->glGenBuffers(1, &glowVbo_);
    gl->glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
    gl->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    setupGlowAttributes(gl);
    gl->glBindVertexArray(0);

    gl->glGenVertexArrays(1, &skyVao_);

    // Bordi dell'arena + assi cartesiani sul pavimento.
    std::vector<float> lineData;  // x,y,z, r,g,b,a
    auto addLine = [&lineData](Vec3 a, Vec3 b, Vec3 color, float alpha) {
        for (const Vec3& p : {a, b}) {
            lineData.insert(lineData.end(), {p.x, p.y, p.z, color.x, color.y, color.z, alpha});
        }
    };

    const Vec3 h = World::kHalf;
    const Vec3 edgeColor{0.32f, 0.86f, 0.96f};
    const Vec3 corners[8] = {{-h.x, -h.y, -h.z}, {h.x, -h.y, -h.z}, {h.x, h.y, -h.z},
                             {-h.x, h.y, -h.z},  {-h.x, -h.y, h.z}, {h.x, -h.y, h.z},
                             {h.x, h.y, h.z},    {-h.x, h.y, h.z}};
    const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                              {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& edge : edges) {
        addLine(corners[edge[0]], corners[edge[1]], edgeColor, 0.55f);
    }

    const float floorY = -h.y + 0.02f;
    addLine({-h.x, floorY, 0.0f}, {h.x, floorY, 0.0f}, {0.95f, 0.32f, 0.32f}, 0.50f);  // X rossa
    addLine({0.0f, floorY, -h.z}, {0.0f, floorY, h.z}, {0.35f, 0.55f, 1.0f}, 0.50f);   // Z blu
    addLine({0.0f, -h.y, 0.0f}, {0.0f, h.y, 0.0f}, {0.40f, 0.95f, 0.55f}, 0.30f);      // Y verde
    for (float x = -h.x + 4.0f; x < h.x; x += 4.0f) {
        addLine({x, floorY, -0.35f}, {x, floorY, 0.35f}, edgeColor, 0.45f);
    }
    for (float z = -h.z + 4.0f; z < h.z; z += 4.0f) {
        addLine({-0.35f, floorY, z}, {0.35f, floorY, z}, edgeColor, 0.45f);
    }
    lineVertexCount_ = static_cast<GLsizei>(lineData.size() / 7);

    gl->glGenVertexArrays(1, &lineVao_);
    gl->glBindVertexArray(lineVao_);
    gl->glGenBuffers(1, &lineVbo_);
    gl->glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    gl->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(lineData.size() * sizeof(float)),
                     lineData.data(), GL_STATIC_DRAW);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                              reinterpret_cast<void*>(3 * sizeof(float)));
    gl->glBindVertexArray(0);

    gl->glEnable(GL_DEPTH_TEST);
    gl->glEnable(GL_CULL_FACE);
    gl->glCullFace(GL_BACK);

    // Polvere deterministica: stesse posizioni ad ogni avvio.
    auto hash01 = [](int i, float seed) {
        const float value = std::sin(static_cast<float>(i) * 12.9898f + seed * 78.233f) * 43758.5453f;
        return value - std::floor(value);
    };
    for (int i = 0; i < 240; ++i) {
        dust_.push_back({(hash01(i, 1.0f) * 2.0f - 1.0f) * World::kHalf.x * 0.94f,
                         (hash01(i, 2.0f) * 2.0f - 1.0f) * World::kHalf.y * 0.94f,
                         (hash01(i, 3.0f) * 2.0f - 1.0f) * World::kHalf.z * 0.94f});
    }
    return true;
}

void Renderer::setupInstanceAttributes(QOpenGLExtraFunctions* gl, GLuint vao) {
    gl->glBindVertexArray(vao);
    gl->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    const GLsizei stride = sizeof(Instance);

    gl->glEnableVertexAttribArray(2);
    gl->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    gl->glVertexAttribDivisor(2, 1);

    gl->glEnableVertexAttribArray(3);
    gl->glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(3 * sizeof(float)));
    gl->glVertexAttribDivisor(3, 1);

    gl->glEnableVertexAttribArray(4);
    gl->glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(6 * sizeof(float)));
    gl->glVertexAttribDivisor(4, 1);

    gl->glEnableVertexAttribArray(5);
    gl->glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(10 * sizeof(float)));
    gl->glVertexAttribDivisor(5, 1);

    gl->glBindVertexArray(0);
}

void Renderer::setupGlowAttributes(QOpenGLExtraFunctions* gl) {
    gl->glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
    const GLsizei stride = sizeof(GlowInstance);

    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    gl->glVertexAttribDivisor(1, 1);

    gl->glEnableVertexAttribArray(2);
    gl->glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(3 * sizeof(float)));
    gl->glVertexAttribDivisor(2, 1);

    gl->glEnableVertexAttribArray(3);
    gl->glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(4 * sizeof(float)));
    gl->glVertexAttribDivisor(3, 1);
}

void Renderer::shutdown(QOpenGLExtraFunctions* gl) {
    for (glx::GpuMesh& mesh : meshes_) {
        glx::destroy(gl, mesh);
    }
    if (instanceVbo_ != 0) gl->glDeleteBuffers(1, &instanceVbo_);
    if (quadVbo_ != 0) gl->glDeleteBuffers(1, &quadVbo_);
    if (glowVbo_ != 0) gl->glDeleteBuffers(1, &glowVbo_);
    if (lineVbo_ != 0) gl->glDeleteBuffers(1, &lineVbo_);
    if (quadVao_ != 0) gl->glDeleteVertexArrays(1, &quadVao_);
    if (glowVao_ != 0) gl->glDeleteVertexArrays(1, &glowVao_);
    if (skyVao_ != 0) gl->glDeleteVertexArrays(1, &skyVao_);
    if (lineVao_ != 0) gl->glDeleteVertexArrays(1, &lineVao_);
    quadVbo_ = glowVbo_ = lineVbo_ = instanceVbo_ = 0;
    quadVao_ = glowVao_ = skyVao_ = lineVao_ = 0;
    sceneProg_.destroy(gl);
    gridProg_.destroy(gl);
    skyProg_.destroy(gl);
    glowProg_.destroy(gl);
    lineProg_.destroy(gl);
}

void Renderer::add(MeshShape shape, Vec3 position, Vec3 scale, Vec3 color, float emissive,
                   float yaw, float pitch) {
    const int index = static_cast<int>(shape);
    if (index < 0 || index >= kShapeCount) {
        return;
    }
    Instance instance{};
    instance.pos[0] = position.x;
    instance.pos[1] = position.y;
    instance.pos[2] = position.z;
    instance.scale[0] = scale.x;
    instance.scale[1] = scale.y;
    instance.scale[2] = scale.z;
    instance.color[0] = color.x;
    instance.color[1] = color.y;
    instance.color[2] = color.z;
    instance.color[3] = emissive;
    instance.rot[0] = yaw;
    instance.rot[1] = pitch;
    batches_[index].push_back(instance);
    ++stats_.instances;
}

void Renderer::addGlow(Vec3 position, float size, Vec3 color, float alpha) {
    GlowInstance glow{};
    glow.pos[0] = position.x;
    glow.pos[1] = position.y;
    glow.pos[2] = position.z;
    glow.size = size;
    glow.color[0] = color.x;
    glow.color[1] = color.y;
    glow.color[2] = color.z;
    glow.color[3] = alpha;
    glows_.push_back(glow);
}

void Renderer::buildScene(const World& world) {
    for (auto& batch : batches_) {
        batch.clear();
    }
    glows_.clear();
    stats_ = {};

    const float time = world.time;
    const int segments = static_cast<int>(world.snake.body.size());

    // --- corpo: dalla coda alla testa ---
    // In prima persona si saltano i primi segmenti: sarebbero dietro la camera
    // e il primo potrebbe entrare nel near plane.
    const int bodyStart = firstPerson_ ? 4 : 1;
    for (int i = segments - 1; i >= bodyStart; --i) {
        const Vec3 position = world.snake.body[static_cast<std::size_t>(i)];
        const float taper = Snake::segmentRadius(i, segments);
        const float radius = taper * (1.0f + 0.05f * std::sin(time * 9.0f - static_cast<float>(i) * 0.65f));
        const float band = 0.5f + 0.5f * std::sin(static_cast<float>(i) * 0.85f);
        Vec3 color = lerp(Vec3{0.13f, 0.66f, 0.34f}, Vec3{0.34f, 0.95f, 0.52f}, band * 0.6f);
        color = tintForEffects(world, color, time);
        if (world.grace > 0.0f && std::fmod(time, 0.26f) < 0.13f) {
            color = color * 0.35f;
        }
        add(MeshShape::Sphere, position, {radius, radius * 0.92f, radius}, color,
            0.10f + 0.20f * world.boostGlow);
    }

    // --- testa e occhi (non in prima persona: la camera è dentro la testa) ---
    if (!firstPerson_) {
        const Vec3 head = world.snake.head;
        const Vec3 forward = world.snake.dir;
        // Base del muso presa dal verme: se è capovolto gli occhi restano
        // attaccati al dorso, invece di scattare all'improvviso.
        Vec3 right = normalize(cross(world.snake.up, forward));
        if (lengthSquared(right) < 1e-5f) {
            right = normalize(cross(Vec3{1.0f, 0.0f, 0.0f}, forward));
        }
        if (lengthSquared(right) < 1e-5f) {
            right = {1.0f, 0.0f, 0.0f};
        }
        const Vec3 up = normalize(cross(forward, right));

        Vec3 headColor = tintForEffects(world, Vec3{0.22f, 0.84f, 0.45f}, time);
        if (world.grace > 0.0f && std::fmod(time, 0.26f) < 0.13f) {
            headColor = headColor * 0.40f;
        }
        add(MeshShape::Sphere, head, {0.74f, 0.70f, 0.74f}, headColor, 0.14f);
        addGlow(head + forward * 0.25f, 2.0f, headColor, 0.22f);

        for (float side : {-1.0f, 1.0f}) {
            // Occhi montati sui fianchi del muso, con pupilla verso l'avanti:
            // da qualunque lato si veda la testa, lo sguardo e' leggibile.
            const Vec3 eye = head + forward * 0.52f + up * 0.30f + right * (0.44f * side);
            add(MeshShape::Sphere, eye, {0.185f, 0.205f, 0.185f}, {0.97f, 0.99f, 1.0f}, 0.30f);
            const Vec3 gaze = normalize(forward * 0.90f + up * 0.06f);
            const Vec3 pupil = eye + gaze * 0.150f;
            add(MeshShape::Sphere, pupil, {0.068f, 0.080f, 0.068f}, {0.02f, 0.04f, 0.05f}, 0.0f);
        }
    }

    // --- cibo ---
    for (const Food& food : world.foods) {
        const FoodDef& def = foodDef(food.kind);
        const float pulse = 1.0f + 0.055f * std::sin(time * 2.6f + food.phase);
        const float bob = 0.14f * std::sin(time * 1.7f + food.phase);
        const Vec3 position = food.position + Vec3{0.0f, bob, 0.0f};
        const Vec3 scale = def.renderScale * (def.radius * pulse);
        const float yaw = time * food.spin + food.phase;
        float pitch = 0.22f * std::sin(time * 1.3f + food.phase);
        if (def.shape == MeshShape::Torus) {
            // La ciambella sta in piedi: di taglio sarebbe solo una linea.
            pitch += 1.15f;
        }

        float emissive = def.emissive * (0.80f + 0.20f * std::sin(time * 3.0f + food.phase));
        float glowAlpha = 0.22f + 0.28f * def.emissive;
        // Cibo in scadenza: lampeggio rapido, il giocatore capisce di affrettarsi.
        if (food.lifespan > 0.0f) {
            const float remaining = food.lifespan - food.age;
            if (remaining < 4.0f) {
                const float blink = 0.5f + 0.5f * std::sin(time * 18.0f);
                emissive *= 0.55f + 0.75f * blink;
                glowAlpha *= 0.55f + 0.65f * blink;
            }
        }

        add(def.shape, position, scale, def.color, emissive, yaw, pitch);
        addGlow(position, def.radius * 3.6f, def.glow, glowAlpha);

        // Piccoli dettagli che rendono leggibile il tipo di frutto.
        if (food.kind == FoodKind::Apple || food.kind == FoodKind::GoldenApple) {
            const Vec3 stem = position + Vec3{0.0f, def.radius * 0.95f, 0.0f};
            add(MeshShape::Sphere, stem, {0.06f, 0.16f, 0.06f}, {0.34f, 0.22f, 0.12f}, 0.05f);
        } else if (food.kind == FoodKind::Bomb) {
            const Vec3 fuse = position + Vec3{0.0f, def.radius * 0.92f, 0.0f};
            add(MeshShape::Sphere, fuse, {0.10f, 0.10f, 0.10f}, {1.0f, 0.55f, 0.12f},
                0.9f + 0.5f * std::sin(time * 14.0f));
            addGlow(fuse, 0.9f, {1.0f, 0.5f, 0.1f}, 0.5f);
        }
    }

    // --- rocce ---
    for (const Rock& rock : world.rocks) {
        const float tilt = 0.10f * std::sin(time * 0.7f + rock.center.x);
        add(MeshShape::Cube, rock.center, rock.half, {0.16f, 0.22f, 0.30f}, 0.035f, rock.yaw, tilt);
    }

    // --- polvere sospesa: da' profondita' allo spazio vuoto ---
    for (const Vec3& base : dust_) {
        const float phase = base.x * 0.7f + base.y * 1.3f + base.z * 0.4f;
        const Vec3 position = base + Vec3{std::sin(time * 0.23f + phase) * 0.55f,
                                          std::sin(time * 0.31f + phase * 1.7f) * 0.42f,
                                          std::cos(time * 0.19f + phase * 0.6f) * 0.55f};
        const float shimmer = 0.06f + 0.035f * std::sin(time * 1.4f + phase * 3.0f);
        addGlow(position, 0.16f + 0.06f * std::sin(phase * 5.0f), {0.55f, 0.86f, 0.95f}, shimmer);
    }

    // --- particelle come aloni additivi ---
    for (const Particle& particle : world.particles) {
        const float lifeRatio = particle.maxLife > 1e-5f ? particle.life / particle.maxLife : 0.0f;
        addGlow(particle.position, particle.size * 2.6f * (0.6f + 0.4f * lifeRatio), particle.color,
                lifeRatio * 0.85f);
    }
}

void Renderer::drawBatches(QOpenGLExtraFunctions* gl) {
    gl->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    for (int i = 0; i < kShapeCount; ++i) {
        if (batches_[i].empty()) {
            continue;
        }
        gl->glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(batches_[i].size() * sizeof(Instance)),
                         batches_[i].data(), GL_DYNAMIC_DRAW);
        gl->glBindVertexArray(meshes_[i].vao);
        gl->glDrawElementsInstanced(GL_TRIANGLES, meshes_[i].indexCount, GL_UNSIGNED_INT, nullptr,
                                    static_cast<GLsizei>(batches_[i].size()));
        ++stats_.drawCalls;
    }
    gl->glBindVertexArray(0);
}

void Renderer::drawGrid(QOpenGLExtraFunctions* gl, const float* viewProjection, Vec3 camPos) {
    gridProg_.use(gl);
    gridProg_.setMat4(gl, "uViewProj", viewProjection);
    gridProg_.set3f(gl, "uCam", camPos.x, camPos.y, camPos.z);
    gridProg_.set3f(gl, "uColor", 0.34f, 0.84f, 0.94f);

    struct Plane {
        Vec3 center;
        Vec3 axisU;
        Vec3 axisV;
        float sizeU;
        float sizeV;
        int grid;
        float strength;
    };
    const Vec3 h = World::kHalf;
    const Plane planes[6] = {
        {{0, -h.y, 0}, {1, 0, 0}, {0, 0, 1}, h.x, h.z, 0, 1.15f},
        {{0, h.y, 0}, {1, 0, 0}, {0, 0, 1}, h.x, h.z, 0, 0.25f},
        {{h.x, 0, 0}, {0, 0, 1}, {0, 1, 0}, h.z, h.y, 2, 0.55f},
        {{-h.x, 0, 0}, {0, 0, 1}, {0, 1, 0}, h.z, h.y, 2, 0.55f},
        {{0, 0, h.z}, {1, 0, 0}, {0, 1, 0}, h.x, h.y, 1, 0.55f},
        {{0, 0, -h.z}, {1, 0, 0}, {0, 1, 0}, h.x, h.y, 1, 0.55f},
    };

    gl->glBindVertexArray(quadVao_);
    for (const Plane& plane : planes) {
        gridProg_.set3f(gl, "uCenter", plane.center.x, plane.center.y, plane.center.z);
        gridProg_.set3f(gl, "uAxisU", plane.axisU.x, plane.axisU.y, plane.axisU.z);
        gridProg_.set3f(gl, "uAxisV", plane.axisV.x, plane.axisV.y, plane.axisV.z);
        gridProg_.set2f(gl, "uSize", plane.sizeU, plane.sizeV);
        gridProg_.setInt(gl, "uGrid", plane.grid);
        gridProg_.set1f(gl, "uStrength", plane.strength);
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        ++stats_.drawCalls;
    }
    gl->glBindVertexArray(0);
}

void Renderer::drawLines(QOpenGLExtraFunctions* gl, const float* viewProjection) {
    lineProg_.use(gl);
    lineProg_.setMat4(gl, "uViewProj", viewProjection);
    lineProg_.set1f(gl, "uAlpha", 0.85f);
    gl->glBindVertexArray(lineVao_);
    gl->glDrawArrays(GL_LINES, 0, lineVertexCount_);
    gl->glBindVertexArray(0);
    ++stats_.drawCalls;
}

void Renderer::drawGlows(QOpenGLExtraFunctions* gl, const float* viewProjection,
                         const Camera& camera) {
    if (glows_.empty()) {
        return;
    }
    const Vec3 forward = normalize(camera.target - camera.position);
    Vec3 right = normalize(cross(forward, camera.up));
    if (lengthSquared(right) < 1e-5f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 up = normalize(cross(right, forward));

    glowProg_.use(gl);
    glowProg_.setMat4(gl, "uViewProj", viewProjection);
    glowProg_.set3f(gl, "uRight", right.x, right.y, right.z);
    glowProg_.set3f(gl, "uUp", up.x, up.y, up.z);
    glowProg_.set3f(gl, "uCam", camera.position.x, camera.position.y, camera.position.z);
    gl->glBindVertexArray(glowVao_);
    gl->glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
    gl->glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(glows_.size() * sizeof(GlowInstance)), glows_.data(),
                     GL_DYNAMIC_DRAW);
    gl->glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(glows_.size()));
    gl->glBindVertexArray(0);
    ++stats_.drawCalls;
}

void Renderer::render(QOpenGLExtraFunctions* gl, const World& world, const Camera& camera,
                      int pixelWidth, int pixelHeight) {
    buildScene(world);

    const int width = std::max(1, pixelWidth);
    const int height = std::max(1, pixelHeight);
    gl->glViewport(0, 0, width, height);
    gl->glClearColor(kFogColor.x, kFogColor.y, kFogColor.z, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl->glEnable(GL_MULTISAMPLE);

    QMatrix4x4 projection;
    projection.perspective(camera.fov, static_cast<float>(width) / static_cast<float>(height),
                           0.08f, 260.0f);
    QMatrix4x4 view;
    view.lookAt(toQt(camera.position), toQt(camera.target), toQt(camera.up));
    const QMatrix4x4 viewProjection = projection * view;
    const float* vp = viewProjection.constData();

    // --- cielo ---
    gl->glDisable(GL_DEPTH_TEST);
    gl->glDepthMask(GL_FALSE);
    gl->glDisable(GL_BLEND);
    const QMatrix4x4 inverseViewProjection = viewProjection.inverted();
    skyProg_.use(gl);
    skyProg_.set1f(gl, "uTime", world.time);
    skyProg_.setMat4(gl, "uInvViewProj", inverseViewProjection.constData());
    skyProg_.set3f(gl, "uCamPos", camera.position.x, camera.position.y, camera.position.z);
    gl->glBindVertexArray(skyVao_);
    gl->glDrawArrays(GL_TRIANGLES, 0, 3);
    ++stats_.drawCalls;

    // --- griglia cartesiana e bordi (additivi, senza scrivere profondita') ---
    gl->glEnable(GL_DEPTH_TEST);
    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    if (showGrid) {
        drawGrid(gl, vp, camera.position);
    }
    drawLines(gl, vp);

    // --- scena opaca ---
    gl->glDisable(GL_BLEND);
    gl->glDepthMask(GL_TRUE);
    gl->glEnable(GL_CULL_FACE);
    sceneProg_.use(gl);
    sceneProg_.setMat4(gl, "uViewProj", vp);
    sceneProg_.set3f(gl, "uCam", camera.position.x, camera.position.y, camera.position.z);
    sceneProg_.set3f(gl, "uKeyDir", 0.45f, 0.85f, 0.35f);
    sceneProg_.set3f(gl, "uFogColor", kFogColor.x, kFogColor.y, kFogColor.z);
    sceneProg_.set1f(gl, "uFogDensity", kFogDensity);
    drawBatches(gl);

    // --- aloni e particelle ---
    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    gl->glDepthMask(GL_FALSE);
    drawGlows(gl, vp, camera);

    gl->glDepthMask(GL_TRUE);
    gl->glDisable(GL_BLEND);
    gl->glDisable(GL_CULL_FACE);
}

}  // namespace w3
