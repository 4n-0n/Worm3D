#version 330 core
// grid.vert — un quad parametrico costruito nel vertex shader.
// Il chiamante sceglie centro, assi e dimensioni: cosi' una sola mesh serve
// per pavimento, soffitto e quattro pareti.
layout(location = 0) in vec2 aCorner;

uniform mat4 uViewProj;
uniform vec3 uCenter;
uniform vec3 uAxisU;
uniform vec3 uAxisV;
uniform vec2 uSize;

out vec3 vWorld;

void main() {
    vec3 world = uCenter + uAxisU * aCorner.x * uSize.x + uAxisV * aCorner.y * uSize.y;
    vWorld = world;
    gl_Position = uViewProj * vec4(world, 1.0);
}
