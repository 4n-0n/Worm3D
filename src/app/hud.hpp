#pragma once
// hud.hpp — HUD disegnato con QPainter sopra il frame OpenGL (testo nitido).

#include "core/world.hpp"

#include <QColor>
#include <QPointF>
#include <QString>

#include <vector>

class QPainter;

namespace w3 {

struct Toast {
    QString text;
    Vec3 color;
    float life = 0.0f;
    float maxLife = 1.0f;
};

struct HudInfo {
    int width = 1600;
    int height = 900;
    int bestScore = 0;
    float fps = 0.0f;
    bool paused = false;
    bool capture = false;
    bool showHelp = true;
    bool autopilot = false;
    bool audioAvailable = true;
    bool muted = false;
    QString cameraLabel;
};

void drawHud(QPainter& painter, const World& world, const std::vector<Toast>& toasts,
             const HudInfo& info);

}  // namespace w3
