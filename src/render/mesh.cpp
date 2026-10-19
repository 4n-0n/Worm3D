#include "render/mesh.hpp"

#include <array>
#include <cmath>

namespace w3 {
namespace {

void addQuad(MeshData& mesh, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const Vec3& p : {a, b, c, d}) {
        mesh.vertices.push_back({p.x, p.y, p.z, normal.x, normal.y, normal.z});
    }
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void addTriangle(MeshData& mesh, Vec3 a, Vec3 b, Vec3 c) {
    const Vec3 n = normalize(cross(b - a, c - a));
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const Vec3& p : {a, b, c}) {
        mesh.vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z});
    }
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2});
}

}  // namespace

MeshData makeSphere(int segments, int rings) {
    MeshData mesh;
    mesh.vertices.reserve(static_cast<std::size_t>((segments + 1) * (rings + 1)));
    for (int r = 0; r <= rings; ++r) {
        const float theta = 3.14159265f * static_cast<float>(r) / static_cast<float>(rings);
        const float y = std::cos(theta);
        const float radius = std::sin(theta);
        for (int s = 0; s <= segments; ++s) {
            const float phi = 6.2831853f * static_cast<float>(s) / static_cast<float>(segments);
            const float x = radius * std::cos(phi);
            const float z = radius * std::sin(phi);
            mesh.vertices.push_back({x, y, z, x, y, z});
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            const std::uint32_t a = static_cast<std::uint32_t>(r * (segments + 1) + s);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(segments + 1);
            const std::uint32_t d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, c, b, d, c});
        }
    }
    return mesh;
}

MeshData makeCube() {
    MeshData mesh;
    addQuad(mesh, {1, -1, 1}, {1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, 0, 0});
    addQuad(mesh, {-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}, {-1, 0, 0});
    addQuad(mesh, {-1, 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}, {0, 1, 0});
    addQuad(mesh, {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}, {0, -1, 0});
    addQuad(mesh, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}, {0, 0, 1});
    addQuad(mesh, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {0, 0, -1});
    return mesh;
}

MeshData makeStar() {
    // Stella a 14 punte: nucleo sferico + coni espliciti. Le punte sono
    // geometricamente appuntite (non una sfera "gonfiata"): si legge subito.
    MeshData mesh;

    const MeshData core = makeSphere(14, 10);
    for (std::size_t i = 0; i + 2 < core.indices.size(); i += 3) {
        const MeshVertex& a = core.vertices[core.indices[i]];
        const MeshVertex& b = core.vertices[core.indices[i + 1]];
        const MeshVertex& c = core.vertices[core.indices[i + 2]];
        addTriangle(mesh, {a.px * 0.46f, a.py * 0.46f, a.pz * 0.46f},
                    {b.px * 0.46f, b.py * 0.46f, b.pz * 0.46f},
                    {c.px * 0.46f, c.py * 0.46f, c.pz * 0.46f});
    }

    std::array<Vec3, 14> directions{};
    std::size_t index = 0;
    for (const Vec3 axis : {Vec3{1, 0, 0}, Vec3{-1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, -1, 0},
                            Vec3{0, 0, 1}, Vec3{0, 0, -1}}) {
        directions[index++] = axis;
    }
    for (float x : {-1.0f, 1.0f}) {
        for (float y : {-1.0f, 1.0f}) {
            for (float z : {-1.0f, 1.0f}) {
                directions[index++] = normalize(Vec3{x, y, z});
            }
        }
    }

    constexpr int kRing = 7;
    constexpr float kBase = 0.30f;
    constexpr float kTip = 1.35f;
    for (const Vec3& dir : directions) {
        const Vec3 reference = std::abs(dir.y) > 0.9f ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
        const Vec3 u = normalize(cross(reference, dir));
        const Vec3 v = cross(dir, u);
        const Vec3 baseCenter = dir * kBase;
        const Vec3 tip = dir * kTip;
        for (int i = 0; i < kRing; ++i) {
            const float a0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(kRing);
            const float a1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(kRing);
            const Vec3 p0 = baseCenter + (u * std::cos(a0) + v * std::sin(a0)) * 0.20f;
            const Vec3 p1 = baseCenter + (u * std::cos(a1) + v * std::sin(a1)) * 0.20f;
            addTriangle(mesh, p0, p1, tip);
        }
    }
    return mesh;
}

MeshData makeOctahedron() {
    const Vec3 x{1, 0, 0};
    const Vec3 y{0, 1, 0};
    const Vec3 z{0, 0, 1};
    const Vec3 nx{-1, 0, 0};
    const Vec3 ny{0, -1, 0};
    const Vec3 nz{0, 0, -1};

    MeshData mesh;
    addTriangle(mesh, y, z, x);
    addTriangle(mesh, y, nx, z);
    addTriangle(mesh, y, nz, nx);
    addTriangle(mesh, y, x, nz);
    addTriangle(mesh, ny, x, z);
    addTriangle(mesh, ny, z, nx);
    addTriangle(mesh, ny, nx, nz);
    addTriangle(mesh, ny, nz, x);
    return mesh;
}

MeshData makeTorus(int majorSegments, int minorSegments) {
    constexpr float kMajor = 0.80f;
    constexpr float kMinor = 0.30f;

    MeshData mesh;
    for (int i = 0; i <= majorSegments; ++i) {
        const float u = 6.2831853f * static_cast<float>(i) / static_cast<float>(majorSegments);
        const Vec3 center{kMajor * std::cos(u), 0.0f, kMajor * std::sin(u)};
        for (int j = 0; j <= minorSegments; ++j) {
            const float v = 6.2831853f * static_cast<float>(j) / static_cast<float>(minorSegments);
            const Vec3 normal{std::cos(v) * std::cos(u), std::sin(v), std::cos(v) * std::sin(u)};
            const Vec3 p = center + normal * kMinor;
            mesh.vertices.push_back({p.x, p.y, p.z, normal.x, normal.y, normal.z});
        }
    }
    for (int i = 0; i < majorSegments; ++i) {
        for (int j = 0; j < minorSegments; ++j) {
            const std::uint32_t a = static_cast<std::uint32_t>(i * (minorSegments + 1) + j);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(minorSegments + 1);
            const std::uint32_t d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, c, b, d, c});
        }
    }
    return mesh;
}

MeshData makeMesh(MeshShape shape) {
    switch (shape) {
        case MeshShape::Sphere: return makeSphere(28, 18);
        case MeshShape::Cube: return makeCube();
        case MeshShape::Star: return makeStar();
        case MeshShape::Octa: return makeOctahedron();
        case MeshShape::Torus: return makeTorus(30, 14);
        case MeshShape::Count: break;
    }
    return makeSphere(16, 10);
}

}  // namespace w3
