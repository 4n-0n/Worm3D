#include "app/game_widget.hpp"

#include "core/autopilot.hpp"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QImage>
#include <QKeyEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdio>

using w3::Cue;
using w3::EffectKind;
using w3::Event;
using w3::EventKind;
using w3::FoodKind;
using w3::RunState;
using w3::Vec3;
using w3::WorldInput;

namespace {

Cue cueForFood(FoodKind kind) {
    switch (kind) {
        case FoodKind::Apple: return Cue::EatApple;
        case FoodKind::GoldenApple: return Cue::EatGolden;
        case FoodKind::Chili: return Cue::EatChili;
        case FoodKind::Blueberry: return Cue::EatBlueberry;
        case FoodKind::Lemon: return Cue::EatLemon;
        case FoodKind::Poison: return Cue::EatPoison;
        case FoodKind::Star: return Cue::EatStar;
        case FoodKind::Magnet: return Cue::EatMagnet;
        case FoodKind::Bomb: return Cue::EatBomb;
        case FoodKind::Heart: return Cue::EatHeart;
        case FoodKind::Crystal: return Cue::EatCrystal;
    }
    return Cue::Power;
}

}  // namespace

GameWidget::GameWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(960, 540);

    QSettings settings;
    bestScore_ = settings.value(QStringLiteral("bestScore"), 0).toInt();

    cameraPosition_ = {0.0f, 2.0f, -12.0f};
    cameraTarget_ = {0.0f, 0.0f, 3.0f};
    world_.reset(seed_);

    timer_.setInterval(8);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &GameWidget::tick);
    frameClock_.start();
}

GameWidget::~GameWidget() {
    timer_.stop();
    audio_.stop();
    if (context() != nullptr) {
        makeCurrent();
        renderer_.shutdown(context()->extraFunctions());
        doneCurrent();
    }
}

void GameWidget::configureCapture(const CaptureOptions& options) {
    capture_ = options;
    captureFinished_ = false;
}

void GameWidget::startAudio(bool enabled) {
    if (!enabled || capture_.active) {
        return;
    }
    audioStarted_ = audio_.start();
}

void GameWidget::initializeGL() {
    QOpenGLExtraFunctions* gl = context()->extraFunctions();
    QString error;
    if (!renderer_.init(gl, &error)) {
        std::fprintf(stderr, "WORMS 3D: inizializzazione OpenGL fallita: %s\n", qPrintable(error));
        qCritical().noquote() << "inizializzazione OpenGL fallita:" << error;
        if (capture_.active) {
            QCoreApplication::exit(2);
        }
        return;
    }

    startAudio(true);
    frameClock_.restart();

    if (!capture_.active) {
        addToast(QStringLiteral("MANGIA · CRESCI · NON MORDERTI"), Vec3{0.55f, 1.0f, 0.75f});
        toasts_.back().maxLife = 4.5f;
        toasts_.back().life = 4.5f;
    }

    if (capture_.active) {
        QTimer::singleShot(0, this, [this]() { runCapture(); });
    } else {
        timer_.start();
    }
}

void GameWidget::resizeGL(int width, int height) {
    Q_UNUSED(width);
    Q_UNUSED(height);
    // La viewport viene impostata ad ogni frame in pixel del framebuffer.
}

void GameWidget::paintGL() {
    if (context() == nullptr) {
        return;
    }
    QOpenGLExtraFunctions* gl = context()->extraFunctions();
    // Terza persona: la testa si vede. In --scenario showcase la camera è
    // comunque esterna.
    renderer_.setFirstPerson(false);
    const int pixelWidth = static_cast<int>(std::lround(width() * devicePixelRatioF()));
    const int pixelHeight = static_cast<int>(std::lround(height() * devicePixelRatioF()));
    renderer_.render(gl, world_, camera_, pixelWidth, pixelHeight);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    drawOverlay(painter);
}

void GameWidget::drawOverlay(QPainter& painter) {
    w3::HudInfo info;
    info.width = width();
    info.height = height();
    info.bestScore = std::max(bestScore_, world_.score);
    info.fps = fps_;
    info.paused = paused_;
    info.capture = capture_.active;
    info.showHelp = showHelp_;
    info.autopilot = autopilot_ || (capture_.active && capture_.scenario != QStringLiteral("showcase"));
    info.audioAvailable = audioStarted_;
    info.muted = audio_.muted();
    info.cameraLabel = QStringLiteral("TERZA PERSONA");
    w3::drawHud(painter, world_, toasts_, info);
}

void GameWidget::tick() {
    const float dt = std::clamp(frameClock_.restart() / 1000.0f, 0.0f, 0.10f);

    if (!paused_) {
        accumulator_ += dt;
        int guard = 0;
        while (accumulator_ >= kFixedStep && guard++ < 8) {
            stepSimulation(kFixedStep);
            accumulator_ -= kFixedStep;
        }
    }

    updateCamera(dt, false);

    for (w3::Toast& toast : toasts_) {
        toast.life -= dt;
    }
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
                                 [](const w3::Toast& toast) { return toast.life <= 0.0f; }),
                  toasts_.end());

    fpsWindow_ += dt;
    ++fpsFrames_;
    if (fpsWindow_ >= 0.5f) {
        fps_ = static_cast<float>(fpsFrames_) / fpsWindow_;
        fpsWindow_ = 0.0f;
        fpsFrames_ = 0;
    }
    update();
}

void GameWidget::stepSimulation(float dt) {
    WorldInput input;
    if (captureLoop_) {
        // Scenario di verifica: muso sempre in alto, per documentare il giro
        // completo (nessun limite a 90 gradi).
        input = {0.0f, 1.0f, false};
    } else if (autopilot_ || capture_.active) {
        input = w3::autopilotInput(world_);
    } else {
        // Comandi da tastiera: A/D imbardata, S muso su, W muso giù.
        // Lo yaw positivo ruota verso +X, che con la camera dietro è la
        // sinistra dello schermo.
        if (keys_.contains(Qt::Key_A) || keys_.contains(Qt::Key_Left)) input.yaw += 1.0f;
        if (keys_.contains(Qt::Key_D) || keys_.contains(Qt::Key_Right)) input.yaw -= 1.0f;
        if (keys_.contains(Qt::Key_S) || keys_.contains(Qt::Key_Down)) input.pitch += 1.0f;
        if (keys_.contains(Qt::Key_W) || keys_.contains(Qt::Key_Up)) input.pitch -= 1.0f;
        input.boost = keys_.contains(Qt::Key_Shift) || keys_.contains(Qt::Key_Space);
    }
    world_.step(dt, input);
    handleEvents();
}

void GameWidget::handleEvents() {
    for (const Event& event : world_.takeEvents()) {
        switch (event.kind) {
            case EventKind::Eat: {
                const w3::FoodDef& def = w3::foodDef(event.food);
                const QString amount = event.amount >= 0
                                           ? QStringLiteral("+%1").arg(event.amount)
                                           : QStringLiteral("−%1").arg(-event.amount);
                addToast(QStringLiteral("%1  %2").arg(QString::fromUtf8(def.name), amount),
                         def.color);
                audio_.play(cueForFood(event.food));
                if (world_.score > bestScore_) {
                    bestScore_ = world_.score;
                    saveBest();
                }
                break;
            }
            case EventKind::Power: {
                const EffectKind effect = event.effect;
                addToast(QString::fromUtf8(w3::effectName(effect)), w3::effectColor(effect));
                audio_.play(Cue::Power);
                break;
            }
            case EventKind::ExtraLife:
                addToast(QStringLiteral("VITA EXTRA"), Vec3{1.0f, 0.45f, 0.70f});
                audio_.play(Cue::EatHeart);
                break;
            case EventKind::ShieldBreak:
                addToast(QStringLiteral("SCUDO ASSORBITO"), Vec3{0.55f, 0.95f, 1.0f});
                audio_.play(Cue::Power);
                break;
            case EventKind::Explosion:
                addToast(QStringLiteral("BOOM!"), Vec3{1.0f, 0.55f, 0.15f});
                audio_.play(Cue::Explosion);
                break;
            case EventKind::LevelUp:
                addToast(QStringLiteral("LIVELLO %1").arg(event.amount), Vec3{0.65f, 1.0f, 0.75f});
                audio_.play(Cue::LevelUp);
                break;
            case EventKind::Bounce:
                audio_.play(Cue::Bounce);
                break;
            case EventKind::Death:
                audio_.play(Cue::Hurt);
                break;
            case EventKind::GameOver:
                audio_.play(Cue::GameOver);
                saveBest();
                break;
            case EventKind::Respawn:
            case EventKind::RockBreak:
                break;
        }
    }
}

void GameWidget::addToast(const QString& text, Vec3 color) {
    w3::Toast toast;
    toast.text = text;
    toast.color = color;
    toast.maxLife = 1.35f;
    toast.life = toast.maxLife;
    toasts_.push_back(toast);
    if (toasts_.size() > 5) {
        toasts_.erase(toasts_.begin());
    }
}

void GameWidget::saveBest() {
    QSettings settings;
    settings.setValue(QStringLiteral("bestScore"), bestScore_);
}

void GameWidget::updateCamera(float dt, bool snap) {
    if (showcase_) {
        return;
    }
    const Vec3 head = world_.snake.head;
    const Vec3 forward = world_.snake.dir;
    const Vec3 worldUp{0.0f, 1.0f, 0.0f};

    Vec3 right = normalize(cross(worldUp, forward));
    if (w3::lengthSquared(right) < 1e-5f) {
        // Muso verticale: riferimento dal dorso del verme.
        right = normalize(cross(world_.snake.up, forward));
    }
    if (w3::lengthSquared(right) < 1e-5f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 localUp = normalize(cross(forward, right));

    // Terza persona: camera dietro e sopra la testa, sguardo in avanti.
    // Si vede il verme di spalle e il corpo che si allunga alle sue spalle.
    Vec3 desiredPosition = head - forward * 8.6f + localUp * 4.4f;
    const Vec3 desiredTarget = head + forward * 4.2f + localUp * 0.4f;
    Vec3 desiredUp = worldUp;

    const Vec3 viewDirection = normalize(desiredTarget - desiredPosition);
    if (std::abs(dot(viewDirection, worldUp)) > 0.93f) {
        desiredUp = localUp;  // looping verticale: niente gimbal
    }

    // Resta dentro l'arena e non finire dentro una roccia.
    const Vec3 limit = w3::World::kHalf - Vec3{1.0f, 1.0f, 1.0f};
    desiredPosition.x = std::clamp(desiredPosition.x, -limit.x, limit.x);
    desiredPosition.y = std::clamp(desiredPosition.y, -limit.y, limit.y);
    desiredPosition.z = std::clamp(desiredPosition.z, -limit.z, limit.z);

    const Vec3 viewOffset = desiredPosition - head;
    const float distanceToHead = w3::length(viewOffset);
    if (distanceToHead > 1e-4f) {
        const Vec3 direction = viewOffset / distanceToHead;
        float allowed = distanceToHead;
        for (float travelled = 1.5f; travelled <= distanceToHead; travelled += 0.6f) {
            if (world_.collidesWithRock(head + direction * travelled, 0.7f)) {
                allowed = std::max(2.0f, travelled - 1.0f);
                break;
            }
        }
        desiredPosition = head + direction * allowed;
    }

    bool snapNow = snap || !cameraReady_;
    if (!snapNow) {
        const Vec3 currentOffset = cameraPosition_ - head;
        const Vec3 desiredOffset = desiredPosition - head;
        const float currentLength = w3::length(currentOffset);
        const float desiredLength = w3::length(desiredOffset);
        if (currentLength > 1e-3f && desiredLength > 1e-3f &&
            w3::dot(currentOffset / currentLength, desiredOffset / desiredLength) < 0.2f) {
            // Inversione brusca (rimbalzo sul muro): un lerp passerebbe dentro
            // il corpo e coprirebbe lo schermo di verde. Meglio uno stacco.
            snapNow = true;
        }
    }

    if (snapNow) {
        cameraPosition_ = desiredPosition;
        cameraTarget_ = desiredTarget;
        cameraUp_ = desiredUp;
        cameraReady_ = true;
    } else {
        const float k = 1.0f - std::exp(-dt * 8.0f);
        cameraPosition_ = w3::lerp(cameraPosition_, desiredPosition, std::min(1.0f, k));
        cameraTarget_ = w3::lerp(cameraTarget_, desiredTarget, std::min(1.0f, k * 1.2f));
        cameraUp_ = w3::normalize(w3::lerp(cameraUp_, desiredUp, std::min(1.0f, k)));
    }

    Vec3 position = cameraPosition_;
    const float shake = world_.shake * 0.20f;
    if (shake > 0.001f) {
        const float t = world_.time;
        position += Vec3{std::sin(t * 57.0f), std::cos(t * 63.0f), std::sin(t * 41.0f)} * shake;
    }
    camera_.position = position;
    camera_.target = cameraTarget_;
    camera_.up = cameraUp_;
    camera_.fov = 62.0f;
}

void GameWidget::togglePause() {
    if (world_.state == RunState::GameOver) {
        return;
    }
    paused_ = !paused_;
    keys_.clear();
    accumulator_ = 0.0f;
    frameClock_.restart();
}

void GameWidget::resetRun() {
    ++seed_;
    world_.reset(seed_);
    toasts_.clear();
    paused_ = false;
    cameraReady_ = false;
    showcase_ = false;
    accumulator_ = 0.0f;
    frameClock_.restart();
}

void GameWidget::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }
    const int key = event->key();
    if (key == Qt::Key_Q && (event->modifiers() & Qt::ControlModifier)) {
        QApplication::quit();
        return;
    }
    switch (key) {
        case Qt::Key_Escape:
            if (world_.state == RunState::GameOver) {
                resetRun();
            } else {
                togglePause();
            }
            return;
        case Qt::Key_P:
            togglePause();
            return;
        case Qt::Key_R:
            resetRun();
            return;
        case Qt::Key_F: {
            QWidget* top = window();
            if (top->isFullScreen()) {
                top->showNormal();
            } else {
                top->showFullScreen();
            }
            return;
        }
        case Qt::Key_H:
            showHelp_ = !showHelp_;
            return;
        case Qt::Key_M:
            audio_.setMuted(!audio_.muted());
            return;
        case Qt::Key_G:
            renderer_.showGrid = !renderer_.showGrid;
            return;
        case Qt::Key_O:
            autopilot_ = !autopilot_;
            return;
        default:
            keys_.insert(key);
            return;
    }
}

void GameWidget::keyReleaseEvent(QKeyEvent* event) {
    if (!event->isAutoRepeat()) {
        keys_.remove(event->key());
    }
}

// --- modalita' cattura: avanza a passi fissi e salva PNG, poi esce ---
void GameWidget::runCapture() {
    if (captureFinished_) {
        return;
    }
    captureFinished_ = true;

    QDir directory(capture_.directory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        std::fprintf(stderr, "WORMS 3D: impossibile creare %s\n", qPrintable(capture_.directory));
        QCoreApplication::exit(3);
        return;
    }

    if (capture_.scenario == QStringLiteral("showcase")) {
        setupShowcase();
    } else if (capture_.scenario == QStringLiteral("loop")) {
        showcase_ = false;
        captureLoop_ = true;
        world_.reset(capture_.seed);
        world_.rocks.clear();
        world_.particles.clear();
        world_.effects.clearAll();
        world_.grace = 1000.0f;  // isola la rotazione dall'autocollisione
        world_.snake.reset({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 5.0f);
        cameraReady_ = false;
    } else {
        showcase_ = false;
        world_.reset(capture_.seed);
        cameraReady_ = false;
    }

    const int shots = std::max(1, capture_.shots);
    for (int i = 0; i < shots; ++i) {
        const float targetTime =
            capture_.seconds * static_cast<float>(i) / static_cast<float>(std::max(1, shots - 1));
        if (showcase_) {
            world_.time = targetTime;
        } else {
            int guard = 0;
            while (world_.time < targetTime && guard++ < 200000) {
                stepSimulation(kFixedStep);
            }
        }
        updateCamera(1.0f, true);
        const QImage frame = grabFramebuffer();
        const QString path = directory.filePath(
            QStringLiteral("%1_%2.png").arg(capture_.scenario).arg(i, 2, 10, QChar('0')));
        if (!frame.save(path)) {
            std::fprintf(stderr, "WORMS 3D: salvataggio fallito %s\n", qPrintable(path));
        } else {
            qInfo().noquote() << "catturato" << path << frame.size();
        }
    }
    QCoreApplication::quit();
}

void GameWidget::setupShowcase() {
    showcase_ = true;
    world_.reset(7);
    world_.foods.clear();
    world_.rocks.clear();
    world_.particles.clear();
    world_.effects.clearAll();
    world_.score = 1240;
    world_.level = 3;
    world_.eaten = 16;
    world_.lives = 3;
    world_.grace = 0.0f;

    // Verme piegato a mano: mostra segmentazione, occhi e assottigliamento.
    w3::Snake& snake = world_.snake;
    snake.reset({-6.4f, -3.6f, 5.4f}, {1.0f, 0.0f, 0.0f}, 26.0f);
    for (int i = 0; i < 150; ++i) {
        const Vec3 step{0.085f, 0.010f * std::sin(static_cast<float>(i) * 0.18f),
                        0.032f * std::sin(static_cast<float>(i) * 0.075f)};
        snake.dir = w3::normalize(step);
        snake.pushHead(snake.head + step);
    }
    snake.setHeading(snake.dir);
    snake.rebuildBody();

    for (int k = 0; k < w3::kFoodKindCount; ++k) {
        w3::Food food;
        food.kind = static_cast<FoodKind>(k);
        const int column = k % 4;
        const int row = k / 4;
        food.position = {-5.4f + 3.6f * static_cast<float>(column),
                         3.2f - 2.2f * static_cast<float>(row), 0.0f};
        food.phase = static_cast<float>(k) * 0.9f;
        food.spin = 0.9f;
        food.lifespan = 0.0f;
        world_.foods.push_back(food);
    }

    world_.effects.apply(EffectKind::Turbo, 4.0f);
    world_.effects.apply(EffectKind::DoubleScore, 7.0f);
    world_.effects.apply(EffectKind::Invincible, 9.0f);
    world_.effects.apply(EffectKind::Magnet, 6.0f);

    camera_.position = {0.0f, 0.4f, -13.2f};
    camera_.target = {0.0f, 0.2f, 0.0f};
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fov = 52.0f;
    cameraReady_ = true;
}
