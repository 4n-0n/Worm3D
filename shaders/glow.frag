#version 330 core
// glow.frag — nucleo + alone, finestrato prima del bordo del quad.
in vec2 vLocal;
in vec4 vColor;
in vec3 vWorld;

uniform vec3 uCam;

out vec4 fragColor;

void main() {
    float r = length(vLocal);
    float core = exp(-r * r * 7.0);
    float halo = exp(-r * r * 1.7) * 0.50;
    float window = 1.0 - smoothstep(0.80, 1.0, r);
    // Un alone additivo a pochi centimetri dalla camera coprirebbe lo schermo:
    // sfuma gli aloni vicini (particelle appena mangiate, cibo sulla testa).
    float nearFade = smoothstep(0.28, 1.7, distance(vWorld, uCam));
    float alpha = (core + halo) * window * vColor.a * nearFade;
    if (alpha <= 0.001) {
        discard;
    }
    fragColor = vec4(vColor.rgb, alpha);
}
