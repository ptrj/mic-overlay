#include <LayerShellQt/Window>
#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QFileInfo>
#include <QTextStream>
#include <QImage>
#include "micstatemonitor.h"
#include "overlayconfig.h"
#include "overlaycontroller.h"
#include "shutdownhandler.h"
#include "desktopui.h"
#include "autostart.h"

int main(int argc, char *argv[])
{
    // LayerShellQt::Window::get selects layer-shell only for the overlay.
    // Settings, menus and About use normal xdg-shell windows.
    qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("mic-overlay"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MIC_OVERLAY_VERSION));
    app.setDesktopFileName(QStringLiteral("mic-overlay"));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Shows the actual mute state of the default PipeWire microphone."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"configure", "Open the configuration dialog."});
    parser.addOption({"no-tray", "Run only the overlay (automated tests)."});
    parser.addOption({"enable-autostart", "Enable desktop autostart and exit."});
    parser.addOption({"disable-autostart", "Disable desktop autostart and exit."});
    QCommandLineOption duration(QStringLiteral("quit-after"),
                               QStringLiteral("Exit after N seconds (smoke test)."),
                               QStringLiteral("seconds"));
    parser.addOption(duration);
    QCommandLineOption configOption(QStringLiteral("config"),
                                    QStringLiteral("Read an alternative INI configuration file."),
                                    QStringLiteral("path"));
    parser.addOption(configOption);
    QCommandLineOption listScreens(QStringLiteral("list-screens"),
                                   QStringLiteral("List monitor indices and exit."));
    parser.addOption(listScreens);
    QCommandLineOption capture(QStringLiteral("capture"),
                               QStringLiteral("Save the overlay image after one second (diagnostic)."),
                               QStringLiteral("path"));
    parser.addOption(capture);
    parser.process(app);

    if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        qCritical("mic-overlay: requires the Qt Wayland platform plugin");
        return 1;
    }
    if (parser.isSet(listScreens)) {
        QTextStream output(stdout);
        const auto screens = QGuiApplication::screens();
        for (int i = 0; i < screens.size(); ++i) {
            auto *screen = screens.at(i);
            output << i << ": " << screen->name() << " ("
                   << screen->geometry().width() << 'x' << screen->geometry().height()
                   << ", scale " << screen->devicePixelRatio() << ')';
            if (screen == QGuiApplication::primaryScreen()) {
                output << " [primary]";
            }
            output << '\n';
        }
        return 0;
    }
    int quitSeconds = 0;
    if (parser.isSet(duration)) {
        bool valid = false;
        quitSeconds = parser.value(duration).toInt(&valid);
        if (!valid || quitSeconds < 1 || quitSeconds > 86400) {
            qCritical("mic-overlay: --quit-after must be between 1 and 86400 seconds");
            return 1;
        }
    }

    const QString configPath = QFileInfo(parser.isSet(configOption)
        ? parser.value(configOption) : OverlayConfig::defaultPath()).absoluteFilePath();
    if (parser.isSet(configOption) && !QFileInfo::exists(configPath)) {
        qCritical("mic-overlay: specified configuration file does not exist");
        return 1;
    }
    const QVariantMap config = OverlayConfig::load(configPath);
    if (parser.isSet("enable-autostart") || parser.isSet("disable-autostart")) {
        QString error;
        // Ensure the referenced default configuration exists before autostart.
        if (parser.isSet("enable-autostart") && !QFileInfo::exists(configPath)
            && !OverlayConfig::save(configPath, config, &error)) {
            qCritical().noquote() << error;
            return 1;
        }
        if (!Autostart::setEnabled(parser.isSet("enable-autostart"),
                                  app.applicationFilePath(), configPath, &error)) {
            qCritical().noquote() << error;
            return 1;
        }
        return 0;
    }
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString identity = QStringLiteral("mic-overlay-") + QString::fromLatin1(
        QCryptographicHash::hash(configPath.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
    const QString socketPath = runtime + '/' + identity;
    QLockFile instanceLock(socketPath + ".lock");
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock()) {
        if (instanceLock.error() != QLockFile::LockFailedError) {
            qCritical("mic-overlay: cannot write the application lock in the session runtime directory");
            return 1;
        }
        QLocalSocket existing;
        existing.connectToServer(socketPath);
        if (existing.waitForConnected(1000)) {
            existing.write("configure\n");
            existing.waitForBytesWritten(1000);
            return 0;
        }
        qCritical("mic-overlay: another instance is running but could not be reached");
        return 1;
    }
    QLocalServer instance;
    instance.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(socketPath);
    if (!instance.listen(socketPath)) {
        qCritical("mic-overlay: could not create the local application socket");
        return 1;
    }
    ShutdownHandler shutdown;
    if (!shutdown.install(app)) {
        qCritical("mic-overlay: could not initialize termination handling");
        return 1;
    }
    MicStateMonitor monitor;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("micState"), QVariant::fromValue(&monitor)},
                                 {QStringLiteral("overlayConfig"), config}});
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) {
        qCritical("mic-overlay: QML root must be a window");
        return 1;
    }
    // QML stays invisible until all layer-shell and input settings are in place.
    window->setFlags(Qt::Tool | Qt::FramelessWindowHint
                     | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
    auto *layer = LayerShellQt::Window::get(window);
    if (!layer) {
        qCritical("mic-overlay: could not configure layer-shell window");
        return 1;
    }
    layer->setScope(QStringLiteral("mic-overlay"));
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)
                      | LayerShellQt::Window::AnchorLeft);
    layer->setExclusiveZone(-1);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layer->setActivateOnShow(false);
    OverlayController controller(window, layer, config.value("screen").toString());
    QObject::connect(window, SIGNAL(indicatorSizeChanged()), &controller, SLOT(refreshGeometry()));
    controller.setPosition(config.value("x").toInt(), config.value("y").toInt());
    std::unique_ptr<DesktopUi> desktop;
    if (!parser.isSet("no-tray")) {
        desktop = std::make_unique<DesktopUi>(&monitor, configPath);
        QObject::connect(desktop.get(), &DesktopUi::configurationChanged,
                         &controller, &OverlayController::applyConfiguration);
        QObject::connect(desktop.get(), &DesktopUi::previewChanged,
                         &controller, &OverlayController::previewSetting);
        QObject::connect(desktop.get(), &DesktopUi::editingChanged,
                         &controller, &OverlayController::setEditing);
        QObject::connect(desktop.get(), &DesktopUi::resetPositionRequested,
                         &controller, &OverlayController::resetPosition);
        QObject::connect(window, SIGNAL(repositionRequested(double,double)),
                         &controller, SLOT(moveOverlay(double,double)));
        QObject::connect(window, SIGNAL(repositionFinished()),
                         &controller, SLOT(finishReposition()));
        QObject::connect(&controller, &OverlayController::positionSaveRequested, desktop.get(),
            [window, configPath, ui = desktop.get()] {
                auto values = OverlayConfig::load(configPath);
                const auto preview = window->property("overlayConfig").toMap();
                for (const auto &key : {"x", "y", "screen"}) {
                    values[key] = preview.value(key);
                }
                QString error;
                if (!OverlayConfig::save(configPath, values, &error)) {
                    ui->showError(error);
                }
            });
        if (parser.isSet("configure")) { desktop->showConfigure(); }
    }
    QObject::connect(&instance, &QLocalServer::newConnection, &app, [&] {
        while (auto *client = instance.nextPendingConnection()) {
            QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
            QObject::connect(client, &QLocalSocket::readyRead, client, [&, client] {
                QByteArray message = client->property("message").toByteArray() + client->readAll();
                client->setProperty("message", message);
                if (message.contains('\n') || message.size() > 64) {
                    if (message == "configure\n" && desktop) { desktop->showConfigure(); }
                    client->disconnectFromServer();
                }
            });
        }
    });
    // Never show an unconfirmed MIC ON. QML controls visibility once state is known.
    window->setProperty("overlayReady", true);
    monitor.start();
    qInfo("mic-overlay: started");
    if (parser.isSet(capture)) {
        const QString capturePath = parser.value(capture);
        QTimer::singleShot(1000, window, [window, capturePath] {
            qInfo() << "mic-overlay: diagnostic window:" << window->isVisible()
                    << window->isExposed() << window->size() << window->position();
            if (!window->grabWindow().save(capturePath)) {
                qWarning("mic-overlay: could not save overlay capture");
            }
        });
    }
    if (quitSeconds > 0) {
        QTimer::singleShot(quitSeconds * 1000, &app, &QCoreApplication::quit);
    }
    return app.exec();
}
