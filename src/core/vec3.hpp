#pragma once
// vec3.hpp — matematica vettoriale minimale, deliberatamente senza Qt.
//
// Perche' esiste: il mondo di gioco (src/core) deve compilare e girare in un
// test headless che non linka Qt ne' OpenGL. Tutte le funzioni qui sono pure.

#include <algorithm>
#include <cmath>

namespace w3 {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(float s, Vec3 a) { return a * s; }
inline Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline Vec3 operator/(Vec3 a, float s) { return a * (1.0f / s); }
inline Vec3& operator+=(Vec3& a, Vec3 b) { a = a + b; return a; }
inline Vec3& operator-=(Vec3& a, Vec3 b) { a = a - b; return a; }
inline Vec3& operator*=(Vec3& a, float s) { a = a * s; return a; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float lengthSquared(Vec3 v) { return dot(v, v); }
inline float length(Vec3 v) { return std::sqrt(lengthSquared(v)); }

// Zero-safe: un vettore nullo resta nullo invece di produrre NaN.
inline Vec3 normalize(Vec3 v) {
    const float len = length(v);
    if (len < 1e-6f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return v / len;
}

inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

inline float distance(Vec3 a, Vec3 b) { return length(a - b); }
inline float distanceSquared(Vec3 a, Vec3 b) { return lengthSquared(a - b); }

// Rotazione di Rodrigues attorno a un asse unitario.
inline Vec3 rotateAroundAxis(Vec3 v, Vec3 axis, float radians) {
    const Vec3 k = normalize(axis);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.0f - c));
}

inline Vec3 clampLength(Vec3 v, float maxLength) {
    const float len = length(v);
    if (len <= maxLength || len < 1e-6f) {
        return v;
    }
    return v * (maxLength / len);
}

inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
inline float smoothstep(float edge0, float edge1, float x) {
    const float t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace w3
