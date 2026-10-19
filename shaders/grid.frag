#version 330 core
// grid.frag — griglia cartesiana con antialiasing analitico.
in vec3 vWorld;

uniform int uGrid;        // 0 = XZ, 1 = XY, 2 = ZY
uniform vec3 uColor;
uniform float uStrength;
uniform vec3 uCam;

out vec4 fragColor;

float gridMask(vec2 uv, float step, float thickness) {
    vec2 coord = uv / step;
    vec2 g = abs(fract(coord) - 0.5) / max(fwidth(coord), vec2(1e-5));
    float line = 1.0 - min(min(g.x, g.y) / thickness, 1.0);
    return line;
}

void main() {
    vec2 uv = uGrid == 0 ? vWorld.xz : (uGrid == 1 ? vWorld.xy : vWorld.zy);

    float minor = gridMask(uv, 1.0, 1.0) * 0.22;
    float major = gridMask(uv, 4.0, 1.6) * 0.65;
    float alpha = (minor + major) * uStrength;

    // Svanisce lontano dal centro e con la distanza dalla camera.
    float radial = exp(-dot(vWorld, vWorld) / (26.0 * 26.0));
    float camFade = exp(-length(vWorld - uCam) * 0.018);
    alpha *= radial * camFade;

    if (alpha <= 0.001) {
        discard;
    }
    fragColor = vec4(uColor * alpha, alpha);
}
