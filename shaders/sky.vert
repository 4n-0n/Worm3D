#version 330 core
// sky.vert — triangolo a schermo intero. Non basta un gradiente: calcoliamo
// il raggio mondo di ogni pixel, così stelle e nebulose restano ferme nello
// spazio e ruotano col verme (fondo cosmico vero, non un'immagine di sfondo).
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;

out vec3 vRay;

void main() {
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vec2 ndc = pos * 2.0 - 1.0;
    vec4 farPoint = uInvViewProj * vec4(ndc, 1.0, 1.0);
    vRay = farPoint.xyz / farPoint.w - uCamPos;
    gl_Position = vec4(ndc, 0.9999, 1.0);
}
