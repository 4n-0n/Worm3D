#version 330 core
// line.vert — linee colorate per bordi dell'arena e assi cartesiani.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

uniform mat4 uViewProj;

out vec4 vColor;

void main() {
    vColor = aColor;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
