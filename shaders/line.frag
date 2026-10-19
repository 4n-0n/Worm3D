#version 330 core
// line.frag
in vec4 vColor;
uniform float uAlpha;

out vec4 fragColor;

void main() {
    fragColor = vec4(vColor.rgb, vColor.a * uAlpha);
}
