#version 330 core
// scene.vert — mesh istanziata: posizione/scala/colore/rotazione per istanza.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 iPos;
layout(location = 3) in vec3 iScale;
layout(location = 4) in vec4 iColor;   // rgb + emissive
layout(location = 5) in vec2 iRot;     // yaw, pitch

uniform mat4 uViewProj;

out vec3 vNormal;
out vec3 vWorld;
out vec4 vColor;

vec3 rotY(vec3 v, float a) {
    float c = cos(a);
    float s = sin(a);
    return vec3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}

vec3 rotX(vec3 v, float a) {
    float c = cos(a);
    float s = sin(a);
    return vec3(v.x, v.y * c - v.z * s, v.y * s + v.z * c);
}

void main() {
    vec3 p = rotX(aPos, iRot.y);
    p = rotY(p, iRot.x);
    vec3 n = rotX(aNormal, iRot.y);
    n = rotY(n, iRot.x);

    vec3 s = max(iScale, vec3(1e-4));
    vec3 world = iPos + p * s;

    // Normale corretta per scala non uniforme (inversa trasposta diagonale).
    vNormal = normalize(n / s);
    vWorld = world;
    vColor = iColor;
    gl_Position = uViewProj * vec4(world, 1.0);
}
