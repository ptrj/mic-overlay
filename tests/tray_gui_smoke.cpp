// Exercise the real application with isolated settings on a real Wayland desktop.
#define main overlayApplicationMain
#include "../src/main.cpp"
#undef main
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSlider>
#include <QtTest>

static void require(bool condition, const char *message)
{
    if (!condition) { qFatal("FAIL: %s", message); }
}

static void installDriver()
{
    QTimer::singleShot(400, [] {
        QDialog *dialog = nullptr;
        QQuickWindow *overlay = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() == "configureDialog") { dialog = qobject_cast<QDialog *>(widget); }
        }
        for (auto *window : QGuiApplication::topLevelWindows()) {
            if (window->title() == "mic-overlay") { overlay = qobject_cast<QQuickWindow *>(window); }
        }
        require(dialog && overlay, "settings and overlay exist");
        require(overlay->property("editing").toBool(), "Configure enables editing");
        require(!overlay->flags().testFlag(Qt::WindowTransparentForInput), "editing accepts mouse input");
        require(overlay->flags().testFlag(Qt::WindowDoesNotAcceptFocus), "overlay still rejects keyboard focus");
        QProcess second;
        second.start(QCoreApplication::applicationFilePath(), {"--configure"});
        require(second.waitForFinished(3000) && second.exitCode() == 0,
                "second launch contacts the existing instance instead of creating another overlay");
        require(!dialog->findChild<QComboBox *>("position"), "corner selector removed");
        require(!dialog->findChild<QSpinBox *>("marginTop"), "margin controls removed");
        dialog->findChild<QPushButton *>("resetPosition")->click();
        const auto centered = OverlayConfig::load(OverlayConfig::defaultPath());
        require(centered.value("x").toInt() == -1 && centered.value("y").toInt() == -1,
                "reset saves centered position");
        const auto size = overlay->screen()->geometry().size();
        const int startX = overlay->property("overlayX").toInt();
        const int startY = overlay->property("overlayY").toInt();
        require(startX == (size.width() - qCeil(overlay->property("indicatorWidth").toDouble())) / 2
                && startY == (size.height() - qCeil(overlay->property("indicatorHeight").toDouble())) / 2,
                "reset centers on monitor");
        require(overlay->size() == size, "editing canvas stays fixed on the monitor");
        require(overlay->mask().boundingRect().topLeft() == QPoint(startX, startY),
                "only the indicator accepts mouse input");
        QSignalSpy frames(overlay, &QQuickWindow::frameSwapped);
        const QPoint grab(startX + 15, startY + 15);
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, grab);
        for (int step = 1; step <= 5; ++step) {
            QTest::mouseMove(overlay, grab + QPoint(step * 2, step * 3), 25);
            require(overlay->property("overlayX").toInt() == startX + step * 2
                    && overlay->property("overlayY").toInt() == startY + step * 3,
                    "drag follows cursor pixel for pixel while held");
        }
        for (int repeat = 0; repeat < 3; ++repeat) {
            QTest::mouseMove(overlay, grab + QPoint(10, 15), 25);
        }
        require(overlay->property("overlayX").toInt() == startX + 10
                && overlay->property("overlayY").toInt() == startY + 15,
                "repeated pointer coordinates do not accumulate movement");
        require(!frames.isEmpty(), "overlay renders movement before mouse release");
        require(LayerShellQt::Window::get(overlay)->margins() == QMargins(),
                "drag does not move the editing surface");
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, grab + QPoint(10, 15));
        const auto position = OverlayConfig::load(OverlayConfig::defaultPath());
        require(position.value("x").toInt() == startX + 10
                && position.value("y").toInt() == startY + 15, "exact drag position saved");
        dialog->findChild<QSlider *>("opacity")->setValue(35);
        dialog->findChild<QCheckBox *>("iconOnly")->setChecked(true);
        QTimer::singleShot(200, [dialog, overlay] {
            const auto preview = overlay->property("overlayConfig").toMap();
            require(preview.value("opacity").toDouble() == 0.35, "live opacity preview");
            require(preview.value("iconOnly").toBool() && overlay->property("indicatorWidth").toDouble() < 150, "icon-only shrinks the overlay");
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
            dialog->reject();
            require(!overlay->property("editing").toBool(), "closing settings disables editing");
            require(overlay->flags().testFlag(Qt::WindowTransparentForInput), "closing settings restores click-through");
            const auto saved = OverlayConfig::load(OverlayConfig::defaultPath());
            require(saved.value("iconOnly").toBool() && saved.value("opacity").toDouble() == 0.35,
                    "appearance persisted");
            require(overlay->width() < 150 && overlay->height() < 100,
                    "closing Configure restores compact surface");
            require(overlay->mask().isEmpty(), "editing mask cleared after locking");
            qInfo("PASS: real settings, free dragging, centering, persistence, opacity, icon-only and relocking");
            QTimer::singleShot(300, qApp, &QCoreApplication::quit);
        });
    });
}
Q_COREAPP_STARTUP_FUNCTION(installDriver)

int main(int argc, char **argv)
{
    return overlayApplicationMain(argc, argv);
}
