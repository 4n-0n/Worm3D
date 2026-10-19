#pragma once
// snake.hpp — corpo del verme come polilinea di scia.
//
// Modello: la testa avanza liberamente; il corpo e' la storia del suo percorso,
// campionata a distanza fissa (kSpacing). La lunghezza in "unita' segmento"
// decide quanti campioni disegnare e quanti contano per l'autocollisione.
//
// Invarianti:
//   - path.front() == head (la testa e' sempre il punto piu' recente)
//   - path e' ordinato dal piu' recente al piu' vecchio
//   - body[i] dista ~i*kSpacing dalla testa lungo il percorso

#include "core/vec3.hpp"

#include <vector>

namespace w3 {

struct Snake {
    static constexpr float kSpacing = 0.40f;
    static constexpr float kHeadRadius = 0.62f;
    static constexpr float kMinLength = 3.0f;
    static constexpr float kMaxLength = 260.0f;

    Vec3 head{0.0f, 0.0f, 0.0f};
    Vec3 dir{0.0f, 0.0f, 1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};  // dorso del verme: ruota con il beccheggio
    float length = 6.0f;

    std::vector<Vec3> path;  // scia, fronte = testa
    std::vector<Vec3> body;  // campioni equidistanti, body[0] ~ testa

    int segmentCount() const;
    void reset(Vec3 start, Vec3 heading, float initialLength);
    // Imposta la direzione e riallinea "up" perpendicolarmente (senza roll).
    void setHeading(Vec3 heading);
    void pushHead(Vec3 newHead);
    void rebuildBody();

    // Raggio del segmento i-esimo: la coda si assottiglia.
    static float segmentRadius(int index, int count);
};

}  // namespace w3
