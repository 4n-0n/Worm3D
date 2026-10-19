#include "app/hud.hpp"

#include "core/food.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace w3 {
namespace {

const QColor kPanel(7, 14, 20, 205);
const QColor kPanelBorder(96, 226, 184, 70);
const QColor kAccent(112, 255, 190);
const QColor kText(228, 242, 238);
const QColor kDim(132, 162, 160);
const QColor kDanger(255, 92, 84);

QFont mono(int pixelSize, QFont::Weight weight = QFont::Normal) {
    QFont font(QStringLiteral("DejaVu Sans Mono"));
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(std::max(7, pixelSize));
    font.setWeight(weight);
    return font;
}

QString number(int value) { return QLocale(QLocale::Italian).toString(value); }

void panel(QPainter& painter, QRectF rect) {
    painter.setPen(QPen(kPanelBorder, 1.0));
    painter.setBrush(kPanel);
    painter.drawRoundedRect(rect, 9.0, 9.0);
}

void label(QPainter& painter, QRectF rect, const QString& text, const QFont& font,
           const QColor& color, int flags = Qt::AlignLeft | Qt::AlignVCenter) {
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(rect, flags, text);
}

void bar(QPainter& painter, QRectF rect, float ratio, const QColor& color) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 26));
    painter.drawRoundedRect(rect, rect.height() * 0.5, rect.height() * 0.5);
    if (ratio > 0.0f) {
        QRectF filled = rect;
        filled.setWidth(std::max(2.0, rect.width() * std::clamp(ratio, 0.0f, 1.0f)));
        painter.setBrush(color);
        painter.drawRoundedRect(filled, rect.height() * 0.5, rect.height() * 0.5);
    }
}

void diamond(QPainter& painter, QPointF center, float radius, const QColor& color) {
    QPolygonF shape;
    shape << QPointF(center.x(), center.y() - radius) << QPointF(center.x() + radius, center.y())
          << QPointF(center.x(), center.y() + radius) << QPointF(center.x() - radius, center.y());
    painter.setPen(QPen(color.darker(160), 1.0));
    painter.setBrush(color);
    painter.drawPolygon(shape);
}

void drawToasts(QPainter& painter, const std::vector<Toast>& toasts, float u, int width) {
    float y = 96.0f * u;
    for (auto it = toasts.rbegin(); it != toasts.rend(); ++it) {
        const Toast& toast = *it;
        const float alpha = std::clamp(toast.life / std::max(0.15f, toast.maxLife * 0.45f), 0.0f, 1.0f);
        const float rise = (1.0f - alpha) * 10.0f * u;
        QFont font = mono(static_cast<int>(14.0f * u), QFont::DemiBold);
        painter.setFont(font);
        const QFontMetricsF metrics(font);
        const float textWidth = metrics.horizontalAdvance(toast.text);
        const float pillWidth = textWidth + 30.0f * u;
        const float pillHeight = 26.0f * u;
        const QRectF pill(width * 0.5f - pillWidth * 0.5f, y - rise, pillWidth, pillHeight);

        painter.setPen(QPen(QColor(toast.color.x * 255, toast.color.y * 255, toast.color.z * 255,
                                   static_cast<int>(alpha * 120)),
                            1.0));
        painter.setBrush(QColor(6, 12, 16, static_cast<int>(alpha * 200)));
        painter.drawRoundedRect(pill, pillHeight * 0.5, pillHeight * 0.5);

        painter.setPen(QColor(toast.color.x * 255, toast.color.y * 255, toast.color.z * 255,
                              static_cast<int>(alpha * 255)));
        painter.drawText(pill, Qt::AlignCenter, toast.text);
        y += 30.0f * u;
    }
}

void drawMinimap(QPainter& painter, const World& world, QRectF area, float u) {
    panel(painter, area);
    const float headerH = 18.0f * u;
    label(painter, QRectF(area.left() + 10.0f * u, area.top() + 4.0f * u, area.width(), headerH),
          QStringLiteral("MAPPA  X/Z"), mono(static_cast<int>(9.5f * u), QFont::DemiBold), kDim);

    const QRectF inner(area.left() + 10.0f * u, area.top() + headerH + 6.0f * u,
                       area.width() - 20.0f * u, area.height() - headerH - 16.0f * u);
    painter.setPen(QPen(QColor(120, 200, 200, 40), 1.0));
    painter.setBrush(QColor(4, 10, 14, 160));
    painter.drawRect(inner);

    const float halfX = World::kHalf.x;
    const float halfZ = World::kHalf.z;
    auto mapPoint = [&](Vec3 p) {
        return QPointF(inner.left() + (p.x + halfX) / (2.0f * halfX) * inner.width(),
                       inner.top() + (p.z + halfZ) / (2.0f * halfZ) * inner.height());
    };

    // griglia di riferimento
    painter.setPen(QPen(QColor(120, 200, 200, 26), 1.0));
    for (int i = 1; i < 4; ++i) {
        const float tx = inner.left() + inner.width() * static_cast<float>(i) / 4.0f;
        const float ty = inner.top() + inner.height() * static_cast<float>(i) / 4.0f;
        painter.drawLine(QPointF(tx, inner.top()), QPointF(tx, inner.bottom()));
        painter.drawLine(QPointF(inner.left(), ty), QPointF(inner.right(), ty));
    }

    for (const Rock& rock : world.rocks) {
        const QPointF p = mapPoint(rock.center);
        const float w = std::max(2.0f, static_cast<float>(rock.half.x / (2.0f * halfX) * inner.width() * 2.0f));
        const float h = std::max(2.0f, static_cast<float>(rock.half.z / (2.0f * halfZ) * inner.height() * 2.0f));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(120, 150, 170, 130));
        painter.drawRect(QRectF(p.x() - w * 0.5f, p.y() - h * 0.5f, w, h));
    }

    for (const Food& food : world.foods) {
        const FoodDef& def = foodDef(food.kind);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(def.color.x * 255, def.color.y * 255, def.color.z * 255, 225));
        painter.drawEllipse(mapPoint(food.position), 2.4f * u, 2.4f * u);
    }

    // scia del verme
    QPolygonF trail;
    for (std::size_t i = 0; i < world.snake.path.size(); i += 4) {
        trail << mapPoint(world.snake.path[i]);
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(110, 245, 165, 120), 1.4f * u));
    painter.drawPolyline(trail);

    // testa come freccia orientata
    const QPointF headPoint = mapPoint(world.snake.head);
    const float angle = std::atan2(world.snake.dir.z, world.snake.dir.x);
    QPolygonF arrow;
    const float size = 5.0f * u;
    for (int k = 0; k < 3; ++k) {
        const float a = angle + static_cast<float>(k) * 2.0944f;
        arrow << QPointF(headPoint.x() + std::cos(a) * size, headPoint.y() + std::sin(a) * size);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(kAccent);
    painter.drawPolygon(arrow);
}

}  // namespace

void drawHud(QPainter& painter, const World& world, const std::vector<Toast>& toasts,
             const HudInfo& info) {
    const int width = info.width;
    const int height = info.height;
    if (width <= 0 || height <= 0) {
        return;
    }
    const float u = std::clamp(std::min(static_cast<float>(width) / 1280.0f,
                                        static_cast<float>(height) / 720.0f),
                               0.55f, 2.6f);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // --- tinte di stato ---
    if (world.damageFlash > 0.01f) {
        painter.fillRect(0, 0, width, height, QColor(255, 40, 30,
                                                     static_cast<int>(world.damageFlash * 85)));
    }
    if (world.poisonPulse > 0.01f) {
        painter.fillRect(0, 0, width, height, QColor(120, 40, 220,
                                                     static_cast<int>(world.poisonPulse * 65)));
    }

    // --- vignetta ---
    QRadialGradient vignette(width * 0.5f, height * 0.5f, std::max(width, height) * 0.72f);
    vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
    vignette.setColorAt(0.62, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 150));
    painter.fillRect(0, 0, width, height, vignette);

    // --- pannello punteggio ---
    const QRectF scorePanel(24.0f * u, 22.0f * u, 318.0f * u, 146.0f * u);
    panel(painter, scorePanel);

    label(painter, QRectF(scorePanel.left() + 16.0f * u, scorePanel.top() + 10.0f * u, 200 * u,
                          16.0f * u),
          QStringLiteral("WORMS 3D"), mono(static_cast<int>(10.5f * u), QFont::Bold), kAccent);
    label(painter, QRectF(scorePanel.left() + 16.0f * u, scorePanel.top() + 30.0f * u, 120 * u,
                          14.0f * u),
          QStringLiteral("PUNTI"), mono(static_cast<int>(9.0f * u)), kDim);
    label(painter, QRectF(scorePanel.left() + 12.0f * u, scorePanel.top() + 42.0f * u, 240 * u,
                          44.0f * u),
          number(world.score), mono(static_cast<int>(38.0f * u), QFont::Bold), kText);
    label(painter, QRectF(scorePanel.left() + 200.0f * u, scorePanel.top() + 18.0f * u, 104 * u,
                          24.0f * u),
          QStringLiteral("RECORD\n%1").arg(number(info.bestScore)),
          mono(static_cast<int>(9.0f * u)), kDim, Qt::AlignRight | Qt::AlignTop);

    label(painter, QRectF(scorePanel.left() + 16.0f * u, scorePanel.top() + 92.0f * u, 280 * u,
                          16.0f * u),
          QStringLiteral("LIVELLO %1     LUNGHEZZA %2").arg(world.level).arg(world.snake.segmentCount()),
          mono(static_cast<int>(10.0f * u)), kText);

    // vite
    label(painter, QRectF(scorePanel.left() + 16.0f * u, scorePanel.top() + 114.0f * u, 48 * u,
                          16.0f * u),
          QStringLiteral("VITE"), mono(static_cast<int>(9.0f * u)), kDim);
    for (int i = 0; i < world.lives; ++i) {
        diamond(painter, QPointF(scorePanel.left() + (68.0f + 17.0f * static_cast<float>(i)) * u,
                                 scorePanel.top() + 122.0f * u),
                5.2f * u, QColor(255, 92, 108));
    }

    // --- potenziamenti attivi ---
    const int effectOrder[] = {static_cast<int>(EffectKind::Turbo),    static_cast<int>(EffectKind::SlowMo),
                               static_cast<int>(EffectKind::DoubleScore), static_cast<int>(EffectKind::Invincible),
                               static_cast<int>(EffectKind::Magnet),   static_cast<int>(EffectKind::Shield),
                               static_cast<int>(EffectKind::Poisoned)};
    int active = 0;
    for (int index : effectOrder) {
        if (world.effects.timer[index] > 0.0f) {
            ++active;
        }
    }
    if (active > 0) {
        const float panelHeight = (26.0f + 24.0f * static_cast<float>(active)) * u;
        const QRectF effectPanel(width - 24.0f * u - 268.0f * u, 22.0f * u, 268.0f * u, panelHeight);
        panel(painter, effectPanel);
        label(painter, QRectF(effectPanel.left() + 14.0f * u, effectPanel.top() + 6.0f * u, 200 * u,
                              16.0f * u),
              QStringLiteral("EFFETTI"), mono(static_cast<int>(9.0f * u), QFont::Bold), kDim);

        float y = effectPanel.top() + 28.0f * u;
        for (int index : effectOrder) {
            const float remaining = world.effects.timer[index];
            if (remaining <= 0.0f) {
                continue;
            }
            const EffectKind effect = static_cast<EffectKind>(index);
            const Vec3 color = effectColor(effect);
            const QColor qtColor(color.x * 255, color.y * 255, color.z * 255);
            painter.setPen(Qt::NoPen);
            painter.setBrush(qtColor);
            painter.drawEllipse(QPointF(effectPanel.left() + 18.0f * u, y + 8.0f * u), 3.6f * u,
                                3.6f * u);
            label(painter, QRectF(effectPanel.left() + 28.0f * u, y, 120 * u, 16.0f * u),
                  QString::fromUtf8(effectName(effect)),
                  mono(static_cast<int>(9.5f * u), QFont::DemiBold), kText);
            const float total = std::max(0.001f, world.effects.total[index]);
            bar(painter, QRectF(effectPanel.left() + 150.0f * u, y + 6.0f * u, 78.0f * u, 4.0f * u),
                remaining / total, qtColor);
            label(painter, QRectF(effectPanel.right() - 58.0f * u, y, 44 * u, 16.0f * u),
                  QStringLiteral("%1s").arg(remaining, 0, 'f', 1),
                  mono(static_cast<int>(9.0f * u)), kDim, Qt::AlignRight | Qt::AlignVCenter);
            y += 24.0f * u;
        }
    }

    // --- minimappa ---
    const float mapSize = 186.0f * u;
    drawMinimap(painter, world,
                QRectF(width - 24.0f * u - mapSize, height - 24.0f * u - mapSize, mapSize, mapSize),
                u);

    // --- stamina ---
    const float staminaWidth = 260.0f * u;
    const QRectF staminaRect(width * 0.5f - staminaWidth * 0.5f, height - 52.0f * u, staminaWidth,
                             9.0f * u);
    label(painter, QRectF(staminaRect.left(), staminaRect.top() - 18.0f * u, staminaWidth,
                          14.0f * u),
          QStringLiteral("SCATTO  (SHIFT)"), mono(static_cast<int>(8.5f * u)), kDim,
          Qt::AlignLeft | Qt::AlignVCenter);
    bar(painter, staminaRect, world.stamina / World::kStaminaMax,
        world.stamina > 0.5f ? QColor(110, 230, 255) : QColor(255, 140, 90));

    // --- aiuto comandi ---
    if (info.showHelp && !info.capture) {
        const QStringList lines{
            QStringLiteral("A / D  imbardata          S / W  muso su · giù"),
            QStringLiteral("SHIFT  scatto             P  pausa"),
            QStringLiteral("R ricomincia   F schermo intero"),
            QStringLiteral("H aiuto   M audio   G griglia   O autopilota"),
        };
        const QRectF helpPanel(24.0f * u, height - (24.0f + 96.0f) * u, 372.0f * u, 96.0f * u);
        panel(painter, helpPanel);
        float y = helpPanel.top() + 10.0f * u;
        for (const QString& line : lines) {
            label(painter, QRectF(helpPanel.left() + 14.0f * u, y, helpPanel.width(), 18.0f * u),
                  line, mono(static_cast<int>(9.5f * u)), kDim);
            y += 19.0f * u;
        }
    }

    // --- stato in alto a destra ---
    QString status;
    if (!info.cameraLabel.isEmpty()) {
        status += QStringLiteral("CAMERA %1  ").arg(info.cameraLabel);
    }
    if (info.autopilot) {
        status += QStringLiteral("AUTOPILOTA  ");
    }
    if (!info.audioAvailable) {
        status += QStringLiteral("AUDIO N/D  ");
    } else if (info.muted) {
        status += QStringLiteral("MUTO  ");
    }
    status += QStringLiteral("%1 FPS").arg(info.fps, 0, 'f', 0);
    label(painter, QRectF(width - 300.0f * u, height - 26.0f * u, 276 * u, 16.0f * u), status,
          mono(static_cast<int>(8.5f * u)), QColor(120, 150, 150, 170),
          Qt::AlignRight | Qt::AlignVCenter);

    drawToasts(painter, toasts, u, width);

    // --- mirino fisso al centro: e' la direzione della testa ---
    {
        const QPointF center(width * 0.5, height * 0.5);
        const float gap = 5.0f * u;
        const float length = 9.0f * u;
        painter.setPen(QPen(QColor(112, 255, 190, 200), 1.4));
        painter.drawLine(center + QPointF(-gap - length, 0.0), center + QPointF(-gap, 0.0));
        painter.drawLine(center + QPointF(gap, 0.0), center + QPointF(gap + length, 0.0));
        painter.drawLine(center + QPointF(0.0, -gap - length), center + QPointF(0.0, -gap));
        painter.drawLine(center + QPointF(0.0, gap), center + QPointF(0.0, gap + length));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(112, 255, 190, 230));
        painter.drawEllipse(center, 1.6 * u, 1.6 * u);
    }

    // --- pausa ---
    if (info.paused) {
        painter.fillRect(0, 0, width, height, QColor(3, 8, 12, 165));
        const QRectF box(width * 0.5f - 230.0f * u, height * 0.5f - 96.0f * u, 460.0f * u,
                         192.0f * u);
        panel(painter, box);
        label(painter, box.adjusted(0, 24.0f * u, 0, 0), QStringLiteral("PAUSA"),
              mono(static_cast<int>(34.0f * u), QFont::Bold), kText, Qt::AlignHCenter | Qt::AlignTop);
        label(painter, box.adjusted(0, 92.0f * u, 0, 0),
              QStringLiteral("P per riprendere · R per ricominciare\nC camera · F schermo intero"),
              mono(static_cast<int>(10.0f * u)), kDim, Qt::AlignHCenter | Qt::AlignTop);
    }

    // --- game over ---
    if (world.state == RunState::GameOver) {
        painter.fillRect(0, 0, width, height, QColor(3, 8, 12, 185));
        const QRectF box(width * 0.5f - 280.0f * u, height * 0.5f - 150.0f * u, 560.0f * u,
                         300.0f * u);
        panel(painter, box);
        label(painter, box.adjusted(0, 26.0f * u, 0, 0), QStringLiteral("GAME OVER"),
              mono(static_cast<int>(36.0f * u), QFont::Bold), QColor(255, 110, 100),
              Qt::AlignHCenter | Qt::AlignTop);
        label(painter, box.adjusted(0, 84.0f * u, 0, 0),
              QStringLiteral("PUNTI  %1").arg(number(world.score)),
              mono(static_cast<int>(18.0f * u), QFont::DemiBold), kText,
              Qt::AlignHCenter | Qt::AlignTop);
        label(painter, box.adjusted(0, 118.0f * u, 0, 0),
              QStringLiteral("RECORD  %1      LUNGHEZZA  %2      LIVELLO  %3")
                  .arg(number(info.bestScore))
                  .arg(world.snake.segmentCount())
                  .arg(world.level),
              mono(static_cast<int>(10.0f * u)), kDim, Qt::AlignHCenter | Qt::AlignTop);
        label(painter, box.adjusted(0, 190.0f * u, 0, 0), QStringLiteral("R per ricominciare"),
              mono(static_cast<int>(12.0f * u), QFont::DemiBold), kAccent,
              Qt::AlignHCenter | Qt::AlignTop);
    }

    painter.restore();
}

}  // namespace w3
