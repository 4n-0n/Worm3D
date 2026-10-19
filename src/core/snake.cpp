#include "core/snake.hpp"

#include <algorithm>
#include <cmath>

namespace w3 {

int Snake::segmentCount() const {
    return std::clamp(static_cast<int>(std::lround(length)), 3, 400);
}

void Snake::reset(Vec3 start, Vec3 heading, float initialLength) {
    head = start;
    up = {0.0f, 1.0f, 0.0f};
    setHeading(heading);
    length = std::clamp(initialLength, kMinLength, kMaxLength);

    // Coda dritta alle spalle: la scia deve esistere subito, altrimenti il
    // primo frame mostrerebbe il corpo collassato sulla testa.
    path.clear();
    const int samples = std::max(8, static_cast<int>((length * kSpacing) / 0.25f) + 4);
    for (int i = 0; i < samples; ++i) {
        path.push_back(head - dir * (static_cast<float>(i) * 0.25f));
    }
    rebuildBody();
}

void Snake::setHeading(Vec3 heading) {
    const Vec3 normalized = normalize(heading);
    dir = lengthSquared(normalized) > 1e-6f ? normalized : Vec3{0.0f, 0.0f, 1.0f};

    // Mantiene "up" perpendicolare alla direzione, proiettando l'up attuale.
    // Serve dopo un reset o dopo aver assegnato una direzione a mano: senza
    // questo, occhi e camera userebbero una base non ortogonale.
    Vec3 reference = lengthSquared(up) > 1e-6f ? up : Vec3{0.0f, 1.0f, 0.0f};
    Vec3 projected = reference - dir * dot(reference, dir);
    if (lengthSquared(projected) < 1e-6f) {
        const Vec3 fallback = std::abs(dir.y) > 0.9f ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{0.0f, 1.0f, 0.0f};
        projected = fallback - dir * dot(fallback, dir);
    }
    up = normalize(projected);
}

void Snake::pushHead(Vec3 newHead) {
    head = newHead;
    if (path.empty()) {
        path.push_back(newHead);
        return;
    }
    // Il punto in testa e' sempre la posizione esatta della testa: la scia si
    // costruisce inserendo in fronte, mai sostituendo (a 120 Hz lo spostamento
    // per passo e' ~0.05 unita': una soglia piu' alta cancellerebbe la storia).
    if (path.empty() || distance(path.front(), newHead) > 1e-5f) {
        path.insert(path.begin(), newHead);
    }

    // Taglio della coda: la scia deve coprire il corpo piu' un piccolo margine.
    const float needed = (static_cast<float>(segmentCount()) + 6.0f) * kSpacing;
    float accumulated = 0.0f;
    std::size_t keep = 1;
    for (std::size_t i = 1; i < path.size(); ++i) {
        accumulated += distance(path[i - 1], path[i]);
        if (accumulated > needed) {
            break;
        }
        keep = i + 1;
    }
    if (keep < path.size()) {
        path.resize(keep);
    }
}

void Snake::rebuildBody() {
    body.clear();
    if (path.empty()) {
        body.push_back(head);
        return;
    }
    body.reserve(static_cast<std::size_t>(segmentCount()));

    // Distanze cumulate lungo la scia: un solo passaggio per campionare tutti i
    // segmenti (O(punti + segmenti) invece di O(punti * segmenti)).
    std::vector<float> cumulative(path.size(), 0.0f);
    for (std::size_t i = 1; i < path.size(); ++i) {
        cumulative[i] = cumulative[i - 1] + distance(path[i - 1], path[i]);
    }

    const int count = segmentCount();
    std::size_t cursor = 0;
    for (int i = 0; i < count; ++i) {
        const float target = static_cast<float>(i) * kSpacing;
        while (cursor + 1 < path.size() && cumulative[cursor + 1] < target) {
            ++cursor;
        }
        if (cursor + 1 >= path.size()) {
            body.push_back(path.back());
            continue;
        }
        const float span = cumulative[cursor + 1] - cumulative[cursor];
        const float t = span > 1e-6f ? (target - cumulative[cursor]) / span : 0.0f;
        body.push_back(lerp(path[cursor], path[cursor + 1], std::clamp(t, 0.0f, 1.0f)));
    }
}

float Snake::segmentRadius(int index, int count) {
    if (count <= 1) {
        return kHeadRadius * 0.8f;
    }
    const float t = static_cast<float>(index) / static_cast<float>(count - 1);
    // Parte spessa sotto la testa e si assottiglia verso la coda.
    return kHeadRadius * (0.86f - 0.50f * std::pow(t, 0.85f));
}

}  // namespace w3
