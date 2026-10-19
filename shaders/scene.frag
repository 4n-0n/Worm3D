#version 330 core
// scene.frag — lambert + rim + speculare, nebbia esponenziale, tono filmico.
in vec3 vNormal;
in vec3 vWorld;
in vec4 vColor;

uniform vec3 uCam;
uniform vec3 uKeyDir;
uniform vec3 uFogColor;
uniform float uFogDensity;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) {
        N = -N;
    }
    vec3 V = normalize(uCam - vWorld);
    vec3 L = normalize(uKeyDir);

    float ndl = max(dot(N, L), 0.0);
    vec3 ambient = mix(vec3(0.055, 0.080, 0.115), vec3(0.165, 0.285, 0.330), N.y * 0.5 + 0.5);

    vec3 base = vColor.rgb;
    vec3 color = base * (ambient + vec3(1.00, 0.96, 0.88) * ndl * 0.95);

    // Luce di riempimento fredda da dietro: stacca le sagome dal fondo.
    float back = max(dot(N, normalize(vec3(-0.45, 0.30, -0.80))), 0.0);
    color += vec3(0.20, 0.45, 0.65) * back * 0.40 * base;

    vec3 H = normalize(L + V);
    color += vec3(1.00, 0.96, 0.88) * pow(max(dot(N, H), 0.0), 48.0) * 0.40;

    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    color += base * rim * 0.35;

    // Auto-illuminazione (cibo, occhi, scie).
    color += base * vColor.w;

    float dist = length(uCam - vWorld);
    float fog = 1.0 - exp(-pow(dist * uFogDensity, 2.0));
    color = mix(color, uFogColor, clamp(fog, 0.0, 0.85));

    // Tono filmico morbido: evita bianchi bruciati sui glow.
    color = 1.0 - exp(-color * 1.25);
    color = pow(color, vec3(0.90));
    fragColor = vec4(color, 1.0);
}
