#version 330 core
// glow.vert — billboard camera-facing per aloni e particelle additive.
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec3 iPos;
layout(location = 2) in float iSize;
layout(location = 3) in vec4 iColor;

uniform mat4 uViewProj;
uniform vec3 uRight;
uniform vec3 uUp;

out vec2 vLocal;
out vec4 vColor;
out vec3 vWorld;

void main() {
    vLocal = aCorner;
    vColor = iColor;
    vec3 world = iPos + (uRight * aCorner.x + uUp * aCorner.y) * iSize;
    vWorld = world;
    gl_Position = uViewProj * vec4(world, 1.0);
}
