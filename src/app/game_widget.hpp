#pragma once
// game_widget.hpp — finestra OpenGL, input, camera, HUD e modalita' --capture.

#include "app/hud.hpp"
#include "audio/engine.hpp"
#include "core/world.hpp"
#include "render/renderer.hpp"

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QSet>
#include <QString>
#include <QTimer>

#include <vector>

class QKeyEvent;
class QPainter;

class GameWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    struct CaptureOptions {
        bool active = false;
        QString directory;
        QString scenario = QStringLiteral("auto");
        float seconds = 10.0f;
        int shots = 6;
        uint32_t seed = 1u;
    };

    explicit GameWidget(QWidget* parent = nullptr);
    ~GameWidget() override;

    void configureCapture(const CaptureOptions& options);
    void startAudio(bool enabled);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private slots:
    void tick();

private:
    static constexpr float kFixedStep = 1.0f / 120.0f;

    void stepSimulation(float dt);
    void handleEvents();
    void updateCamera(float dt, bool snap);
    void drawOverlay(QPainter& painter);
    void addToast(const QString& text, w3::Vec3 color);
    void resetRun();
    void runCapture();
    void setupShowcase();
    void togglePause();
    void saveBest();

    w3::World world_;
    w3::Renderer renderer_;
    w3::AudioEngine audio_;
    w3::Camera camera_;
    w3::Vec3 cameraPosition_;
    w3::Vec3 cameraTarget_;
    w3::Vec3 cameraUp_{0.0f, 1.0f, 0.0f};
    bool cameraReady_ = false;

    CaptureOptions capture_;
    QTimer timer_;
    QElapsedTimer frameClock_;
    QSet<int> keys_;
    std::vector<w3::Toast> toasts_;

    float accumulator_ = 0.0f;
    float fps_ = 0.0f;
    float fpsWindow_ = 0.0f;
    int fpsFrames_ = 0;
    int bestScore_ = 0;
    uint32_t seed_ = 1u;
    bool paused_ = false;
    bool showHelp_ = true;
    bool autopilot_ = false;
    bool showcase_ = false;
    bool captureLoop_ = false;
    bool captureFinished_ = false;
    bool audioStarted_ = false;
};
