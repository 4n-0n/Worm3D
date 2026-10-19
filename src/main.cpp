// main.cpp — entry point. Il formato OpenGL va scelto PRIMA di QApplication.
//
// Uso:
//   ./worms3d                          gioco normale
//   ./worms3d --fullscreen
//   ./worms3d --size 1600x900 --autopilot
//   ./worms3d --capture /tmp/shots --scenario auto --seconds 12 --shots 6
//   ./worms3d --capture /tmp/shots --scenario showcase --shots 1

#include "app/game_widget.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>

#include <cstdio>

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(0);
    format.setSamples(4);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WORMS 3D"));
    QApplication::setOrganizationName(QStringLiteral("pseudocodice"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("WORMS 3D — serpente tridimensionale in un'arena cartesiana."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption fullscreenOption(QStringLiteral("fullscreen"),
                                              QStringLiteral("Avvia a schermo intero."));
    const QCommandLineOption sizeOption(QStringLiteral("size"),
                                        QStringLiteral("Dimensione finestra, es. 1600x900."),
                                        QStringLiteral("WxH"), QStringLiteral("1440x900"));
    const QCommandLineOption captureOption(QStringLiteral("capture"),
                                           QStringLiteral("Cattura PNG nella cartella ed esce."),
                                           QStringLiteral("dir"));
    const QCommandLineOption scenarioOption(QStringLiteral("scenario"),
                                            QStringLiteral("auto | showcase"), QStringLiteral("nome"),
                                            QStringLiteral("auto"));
    const QCommandLineOption secondsOption(QStringLiteral("seconds"),
                                           QStringLiteral("Durata simulata delle catture."),
                                           QStringLiteral("sec"), QStringLiteral("10"));
    const QCommandLineOption shotsOption(QStringLiteral("shots"),
                                         QStringLiteral("Numero di frame catturati."),
                                         QStringLiteral("n"), QStringLiteral("6"));
    const QCommandLineOption seedOption(QStringLiteral("seed"), QStringLiteral("Seed del mondo."),
                                        QStringLiteral("n"), QStringLiteral("1"));
    const QCommandLineOption autopilotOption(QStringLiteral("autopilot"),
                                             QStringLiteral("Pilota automatico."));
    parser.addOption(fullscreenOption);
    parser.addOption(sizeOption);
    parser.addOption(captureOption);
    parser.addOption(scenarioOption);
    parser.addOption(secondsOption);
    parser.addOption(shotsOption);
    parser.addOption(seedOption);
    parser.addOption(autopilotOption);
    parser.process(app);

    QSize windowSize(1440, 900);
    const QStringList sizeParts = parser.value(sizeOption).split(QLatin1Char('x'));
    if (sizeParts.size() == 2) {
        windowSize = QSize(sizeParts[0].toInt(), sizeParts[1].toInt());
        if (windowSize.width() < 640 || windowSize.height() < 360) {
            windowSize = QSize(1440, 900);
        }
    }

    GameWidget widget;
    widget.setWindowTitle(QStringLiteral("WORMS 3D — il verme che cresce"));
    widget.resize(windowSize);

    GameWidget::CaptureOptions capture;
    if (parser.isSet(captureOption)) {
        capture.active = true;
        capture.directory = parser.value(captureOption);
        capture.scenario = parser.value(scenarioOption);
        capture.seconds = parser.value(secondsOption).toFloat();
        capture.shots = parser.value(shotsOption).toInt();
        capture.seed = parser.value(seedOption).toUInt();
        if (capture.seconds <= 0.1f) {
            capture.seconds = 0.1f;
        }
        widget.configureCapture(capture);
    }

    widget.show();
    if (parser.isSet(fullscreenOption)) {
        widget.showFullScreen();
    }
    return app.exec();
}
