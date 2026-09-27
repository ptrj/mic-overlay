#include "autostart.h"
#include "desktopui.h"
#include "micstatemonitor.h"
#include "overlayconfig.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QtTest>

class DesktopUiTest : public QObject {
    Q_OBJECT
private slots:
    void autostartRoundTrip() {
        QTemporaryDir directory;
        const QString path = directory.filePath("autostart/mic-overlay.desktop");
        QString error;
        QVERIFY(Autostart::setEnabled(true, "/opt/My App/mic-overlay", "/tmp/my config.ini", &error, path));
        QVERIFY(Autostart::enabled(path));
        QFile entry(path);
        QVERIFY(entry.open(QIODevice::ReadOnly));
        const QByteArray data = entry.readAll();
        QVERIFY(data.contains("Exec=\"/opt/My App/mic-overlay\" --config \"/tmp/my config.ini\""));
        QVERIFY(!Autostart::setEnabled(true, "bad\npath", "/tmp/config", &error, path));
        QVERIFY(Autostart::enabled(path));
        QVERIFY(Autostart::desktopEntry("/tmp/a%f", "/tmp/$file").contains("a%%f"));
        QVERIFY(Autostart::setEnabled(false, "", "", &error, path));
        QVERIFY(!Autostart::enabled(path));
        QVERIFY(Autostart::setEnabled(false, "", "", &error, path));
    }
    void configurationAndEditing() {
        QTemporaryDir directory;
        const QByteArray oldConfig = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
        MicStateMonitor monitor(nullptr, "wpctl", 2000, "");
        const QString path = directory.filePath("mic-overlay/config.ini");
        DesktopUi ui(&monitor, path);
        QSignalSpy editing(&ui, &DesktopUi::editingChanged);
        QSignalSpy previews(&ui, &DesktopUi::previewChanged);
        QSignalSpy saved(&ui, &DesktopUi::configurationChanged);
        ui.showConfigure();
        QDialog *dialog = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() == "configureDialog") { dialog = qobject_cast<QDialog *>(widget); }
        }
        QVERIFY(dialog);
        QCOMPARE(editing.last().at(0).toBool(), true);
        auto *opacity = dialog->findChild<QSlider *>("opacity");
        auto *iconOnly = dialog->findChild<QCheckBox *>("iconOnly");
        opacity->setValue(35);
        iconOnly->setChecked(true);
        QCOMPARE(previews.count(), 2);
        dialog->findChild<QSpinBox *>("fontSize")->setValue(22);
        dialog->findChild<QCheckBox *>("autostart")->setChecked(true);
        auto *buttons = dialog->findChild<QDialogButtonBox *>();
        buttons->button(QDialogButtonBox::Apply)->click();
        QVERIFY(!saved.isEmpty());
        const auto config = OverlayConfig::load(path);
        QCOMPARE(config.value("opacity").toDouble(), 0.35);
        QVERIFY(config.value("iconOnly").toBool());
        QCOMPARE(config.value("fontSize").toInt(), 22);
        QVERIFY(Autostart::enabled());
        dialog->reject();
        QCOMPARE(editing.last().at(0).toBool(), false);
        qputenv("XDG_CONFIG_HOME", oldConfig);
    }
};
QTEST_MAIN(DesktopUiTest)
#include "desktopui_test.moc"
