#include "desktopui.h"
#include "autostart.h"
#include "micstatemonitor.h"
#include "overlayconfig.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QSpinBox>
#include <QSlider>
#include <QSvgRenderer>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QVBoxLayout>

DesktopUi::DesktopUi(MicStateMonitor *monitor, const QString &configPath, QObject *parent)
    : QObject(parent), m_monitor(monitor), m_configPath(configPath),
      m_menu(std::make_unique<QMenu>()), m_tray(std::make_unique<QSystemTrayIcon>())
{
    connect(m_menu->addAction(QStringLiteral("Configure…")), &QAction::triggered,
            this, &DesktopUi::showConfigure);
    connect(m_menu->addAction(QStringLiteral("About")), &QAction::triggered, this, [this] {
        auto *about = new QMessageBox(QMessageBox::Information, QStringLiteral("About Microphone Overlay"),
            QStringLiteral("Microphone Overlay %1\n\nShows the actual mute state of your default microphone."
                           "\nQt6 / LayerShellQt / PipeWire\n\nAuthor: ptrj\nContact: aur@dbs.sk"
                           "\nLicense: GPL-3.0-only")
                .arg(QCoreApplication::applicationVersion()), QMessageBox::Ok);
        about->setAttribute(Qt::WA_DeleteOnClose);
        about->setWindowIcon(m_tray->icon());
        about->show();
    });
    m_menu->addSeparator();
    connect(m_menu->addAction(QStringLiteral("Exit")), &QAction::triggered,
            qApp, &QApplication::quit);
    m_tray->setContextMenu(m_menu.get());
    connect(m_tray.get(), &QSystemTrayIcon::activated, this, [this](auto reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showConfigure();
        }
    });
    connect(monitor, &MicStateMonitor::mutedChanged, this, &DesktopUi::updateStatus);
    connect(monitor, &MicStateMonitor::availableChanged, this, &DesktopUi::updateStatus);
    updateStatus();
    m_tray->show();
}
DesktopUi::~DesktopUi() = default;

void DesktopUi::showError(const QString &message)
{
    QMessageBox::warning(m_dialog.get(), "Could not save position", message);
}

void DesktopUi::updateStatus()
{
    const bool muted = m_monitor->available() && m_monitor->muted();
    QPixmap image(32, 32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(!m_monitor->available() ? QColor("#26303c")
                     : QColor(muted ? "#7b202b" : "#166534"));
    painter.drawRoundedRect(QRectF(0, 0, 32, 32), 7, 7);
    QSvgRenderer icon(muted ? QStringLiteral(":/assets/mic-muted.svg")
                           : QStringLiteral(":/assets/mic-on.svg"));
    icon.render(&painter, QRectF(5, 5, 22, 22));
    painter.end();
    m_tray->setIcon(QIcon(image));
    m_tray->setToolTip(!m_monitor->available() ? QStringLiteral("Microphone unavailable")
                     : muted ? QStringLiteral("MIC MUTED") : QStringLiteral("MIC ON"));
}

void DesktopUi::showConfigure()
{
    if (m_dialog && m_dialog->isVisible()) {
        m_dialog->raise();
        m_dialog->activateWindow();
        return;
    }
    m_dialog = std::make_unique<QDialog>();
    auto *dialog = m_dialog.get();
    dialog->setObjectName(QStringLiteral("configureDialog"));
    dialog->setWindowTitle(QStringLiteral("Configure Microphone Overlay"));
    dialog->setWindowIcon(m_tray->icon());
    dialog->setMinimumWidth(480);
    auto *layout = new QVBoxLayout(dialog);
    auto *hint = new QLabel("Drag the overlay while this window is open.\n"
                           "Position is saved when you release the mouse.", dialog);
    layout->addWidget(hint);
    auto *tabs = new QTabWidget(dialog);
    layout->addWidget(tabs);
    auto *general = new QWidget(tabs);
    auto *appearance = new QWidget(tabs);
    tabs->addTab(general, QStringLiteral("Overlay"));
    tabs->addTab(appearance, QStringLiteral("Appearance"));
    auto *form = new QFormLayout(general);
    auto *style = new QFormLayout(appearance);
    const auto config = OverlayConfig::load(m_configPath);
    auto fields = std::make_shared<QMap<QString, QWidget *>>();
    auto add = [&](QFormLayout *target, const QString &label, const QString &key, QWidget *widget) {
        widget->setObjectName(key);
        fields->insert(key, widget);
        target->addRow(label, widget);
    };
    auto *screen = new QComboBox(general);
    screen->addItem("Primary monitor", "primary");
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        screen->addItem(QStringLiteral("%1 — %2").arg(i).arg(screens.at(i)->name()), QString::number(i));
    }
    int selected = screen->findData(config.value("screen"));
    if (selected < 0) {
        screen->addItem(config.value("screen").toString() + " (unavailable)", config.value("screen"));
        selected = screen->count() - 1;
    }
    screen->setCurrentIndex(selected);
    add(form, "Monitor", "screen", screen);
    auto *reset = new QPushButton("Reset position", general);
    reset->setObjectName("resetPosition");
    reset->setToolTip("Center the overlay on the selected monitor");
    form->addRow(reset);
    connect(reset, &QPushButton::clicked, dialog, [this, screen] {
        emit previewChanged("screen", screen->currentData());
        emit resetPositionRequested();
    });
    auto addCheck = [&](QFormLayout *target, const QString &label, const QString &key) {
        auto *check = new QCheckBox(dialog);
        check->setChecked(config.value(key).toBool());
        add(target, label, key, check);
        return check;
    };
    addCheck(form, "Show when microphone is on", "showWhenUnmuted");
    addCheck(form, "Hide when microphone is unavailable", "hideWhenUnavailable");
    addCheck(style, "Bold text", "bold");
    auto *iconOnly = addCheck(form, "Show icon only", "iconOnly");
    connect(iconOnly, &QCheckBox::toggled, dialog, [this](bool value) {
        emit previewChanged("iconOnly", value);
    });
    auto *autostart = new QCheckBox("Start automatically after login", general);
    autostart->setObjectName("autostart");
    autostart->setChecked(Autostart::enabled());
    form->addRow(autostart);
    auto *font = new QSpinBox(appearance);
    font->setRange(8, 72);
    font->setValue(config.value("fontSize").toInt());
    add(style, "Font size", "fontSize", font);
    auto *opacity = new QSlider(Qt::Horizontal, appearance);
    opacity->setRange(0, 100);
    opacity->setValue(qRound(config.value("opacity").toDouble() * 100));
    add(style, "Background opacity", "opacity", opacity);
    auto *percentage = new QLabel(QString::number(opacity->value()) + "%", appearance);
    style->addRow("", percentage);
    connect(opacity, &QSlider::valueChanged, dialog, [this, percentage](int value) {
        percentage->setText(QString::number(value) + "%");
        emit previewChanged("opacity", value / 100.0);
    });
    for (const auto &key : {"mutedText", "unmutedText", "mutedColor", "unmutedColor"}) {
        auto *text = new QLineEdit(config.value(key).toString(), appearance);
        const QString label = QString(key).startsWith("muted") ? "Muted" : "Microphone on";
        if (QString(key).endsWith("Color")) {
            text->setValidator(new QRegularExpressionValidator(QRegularExpression("#[0-9a-fA-F]{6}"), text));
            auto *row = new QWidget(appearance);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->addWidget(text);
            auto *choose = new QPushButton("Choose…", row);
            rowLayout->addWidget(choose);
            connect(choose, &QPushButton::clicked, dialog, [dialog, text] {
                const auto color = QColorDialog::getColor(QColor(text->text()), dialog);
                if (color.isValid()) { text->setText(color.name()); }
            });
            text->setObjectName(key);
            fields->insert(key, text);
            style->addRow(label + " color", row);
        } else {
            add(style, label + " text", key, text);
        }
    }
    for (const auto &key : {"mutedIcon", "unmutedIcon"}) {
        auto *icons = new QComboBox(appearance);
        icons->addItems({"mic-on", "mic-muted"});
        icons->setCurrentText(config.value(key).toString());
        add(style, QString(key) == "mutedIcon" ? "Muted icon" : "Microphone on icon", key, icons);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply
                                        | QDialogButtonBox::Cancel, dialog);
    layout->addWidget(buttons);
    auto save = [this, fields, autostart, dialog] {
        QVariantMap values = OverlayConfig::load(m_configPath);
        for (auto it = fields->begin(); it != fields->end(); ++it) {
            if (auto *spin = qobject_cast<QSpinBox *>(it.value())) { values[it.key()] = spin->value(); }
            else if (auto *slider = qobject_cast<QSlider *>(it.value())) { values[it.key()] = slider->value() / 100.0; }
            else if (auto *check = qobject_cast<QCheckBox *>(it.value())) { values[it.key()] = check->isChecked(); }
            else if (auto *text = qobject_cast<QLineEdit *>(it.value())) {
                if (text->text().trimmed().isEmpty() || !text->hasAcceptableInput()) {
                    QMessageBox::warning(dialog, "Invalid setting", "Check the text and color fields.");
                    return false;
                }
                values[it.key()] = text->text();
            } else if (auto *combo = qobject_cast<QComboBox *>(it.value())) {
                values[it.key()] = it.key() == "screen" ? combo->currentData() : QVariant(combo->currentText());
            }
        }
        QString error;
        if (!OverlayConfig::save(m_configPath, values, &error)) {
            QMessageBox::warning(dialog, "Could not save settings", error);
            return false;
        }
        emit configurationChanged(values);
        if (!Autostart::setEnabled(autostart->isChecked(), QCoreApplication::applicationFilePath(),
                                  m_configPath, &error)) {
            QMessageBox::warning(dialog, "Settings saved; autostart failed", error);
            return false;
        }
        return true;
    };
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, dialog, [save] { save(); });
    connect(buttons, &QDialogButtonBox::accepted, dialog, [save, dialog] { if (save()) { dialog->accept(); } });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialog, &QDialog::finished, this, [this] {
        emit configurationChanged(OverlayConfig::load(m_configPath));
        emit editingChanged(false);
    });
    emit editingChanged(true);
    dialog->show();
}
