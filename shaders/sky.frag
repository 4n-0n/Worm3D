#version 330 core
// sky.frag — fondo cosmico procedurale: nebulose, banda galattica, stelle.
// Tutto è funzione della direzione del raggio: niente seam, nessuna texture.
in vec3 vRay;

uniform float uTime;

out vec4 fragColor;

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float noise3(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i);
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float amplitude = 0.5;
    float sum = 0.0;
    for (int i = 0; i < 4; ++i) {
        sum += amplitude * noise3(p);
        p = p * 2.03 + 17.1;
        amplitude *= 0.5;
    }
    return sum;
}

// Stelle puntiformi: una per cella, con posizione casuale dentro la cella.
float starLayer(vec3 dir, float scale, float threshold, float size) {
    vec3 cell = floor(dir * scale);
    float seed = hash13(cell);
    if (seed < threshold) {
        return 0.0;
    }
    vec3 jitter = vec3(hash13(cell + 1.7), hash13(cell + 4.3), hash13(cell + 9.1));
    vec3 starDir = normalize(cell + jitter);
    float d = length(dir - starDir);
    float twinkle = 0.55 + 0.45 * sin(uTime * (1.2 + 2.5 * seed) + seed * 50.0);
    return smoothstep(size, 0.0, d) * twinkle;
}

void main() {
    vec3 dir = normalize(vRay);

    // Base: nero-blu con un leggero gradiente.
    vec3 col = mix(vec3(0.010, 0.012, 0.030), vec3(0.002, 0.003, 0.010),
                   smoothstep(-0.6, 0.9, dir.y));

    // Nebulose: fbm con domain warping, due strati.
    vec3 p = dir * 2.2;
    float n1 = fbm(p + vec3(0.0, 0.0, uTime * 0.006));
    float n2 = fbm(p * 2.4 + n1 * 1.8 + 5.0);
    float cloud = smoothstep(0.30, 0.86, n1 * 0.65 + n2 * 0.45);
    vec3 nebula = mix(vec3(0.11, 0.02, 0.24), vec3(0.30, 0.08, 0.42), n2);
    nebula += vec3(0.02, 0.14, 0.20) * pow(n2, 3.0);
    col += nebula * cloud * 0.85;

    // Banda galattica: un piano inclinato con polvere luminosa.
    vec3 bandNormal = normalize(vec3(0.30, 0.55, 0.78));
    float band = exp(-pow(dot(dir, bandNormal) / 0.20, 2.0));
    float bandNoise = fbm(dir * 4.5 + 3.0);
    col += vec3(0.16, 0.19, 0.29) * band * (0.30 + 0.70 * bandNoise) * 0.55;

    // Nucleo luminoso lontano: dà un "punto di riferimento" al cielo.
    vec3 coreDir = normalize(vec3(-0.62, 0.28, 0.73));
    col += vec3(0.28, 0.12, 0.42) * exp(-length(dir - coreDir) * 4.5) * 0.20;

    // Stelle: due strati, dimensioni diverse.
    col += vec3(0.80, 0.86, 1.00) * starLayer(dir, 260.0, 0.9955, 0.0022) * 0.55;
    col += vec3(1.00, 0.92, 0.80) * starLayer(dir, 110.0, 0.9985, 0.0040) * 1.30;

    // Polvere sottile.
    col += vec3(0.03, 0.04, 0.08) * pow(fbm(dir * 1.3 + 9.0), 3.0);

    col = 1.0 - exp(-col * 1.35);
    col += (hash13(vec3(gl_FragCoord.xy, uTime)) - 0.5) / 255.0;
    fragColor = vec4(col, 1.0);
}
