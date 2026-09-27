#include "overlayconfig.h"

#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <cmath>

bool OverlayConfig::save(const QString &path, const QVariantMap &values, QString *error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        *error = QStringLiteral("Could not create the configuration directory.");
        return false;
    }
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("overlay"));
    // Remove obsolete corner placement keys when saving the current format.
    for (const auto &key : {"position", "marginTop", "marginRight", "marginBottom", "marginLeft"}) {
        settings.remove(key);
    }
    for (auto it = values.begin(); it != values.end(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        *error = QStringLiteral("Could not save configuration: ") + path;
        return false;
    }
    return true;
}

QString OverlayConfig::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/mic-overlay/config.ini");
}

QVariantMap OverlayConfig::defaults()
{
    return {
        {"x", -1}, {"y", -1},
        {"showWhenUnmuted", true}, {"hideWhenUnavailable", true},
        {"iconOnly", false},
        {"screen", "primary"}, {"opacity", 0.85}, {"fontSize", 15}, {"bold", true},
        {"mutedText", "MIC MUTED"}, {"unmutedText", "MIC ON"},
        {"mutedColor", "#7b202b"}, {"unmutedColor", "#166534"},
        {"mutedIcon", "mic-muted"}, {"unmutedIcon", "mic-on"}
    };
}

QVariantMap OverlayConfig::load(const QString &path)
{
    QVariantMap result = defaults();
    QFile file(path);
    if (!file.exists()) {
        return result;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("mic-overlay: cannot read configuration; using defaults");
        return result;
    }
    file.close();
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("overlay"));
    settings.allKeys(); // Force parsing before checking format errors.
    if (settings.status() != QSettings::NoError) {
        qWarning("mic-overlay: invalid configuration file; using defaults");
        return result;
    }
    for (auto it = result.begin(); it != result.end(); ++it) {
        if (!settings.contains(it.key())) {
            continue;
        }
        // QSettings splits unquoted values at commas into a string list.
        const QVariant stored = settings.value(it.key());
        const QString raw = stored.metaType().id() == QMetaType::QStringList
            ? stored.toStringList().join(QStringLiteral(", ")) : stored.toString();
        const QString value = raw.trimmed();
        bool valid = true;
        QVariant parsed;
        switch (it.value().metaType().id()) {
        case QMetaType::Bool: {
            const QString boolean = value.toLower();
            valid = boolean == "true" || boolean == "false" || boolean == "1" || boolean == "0";
            parsed = boolean == "true" || boolean == "1";
            break;
        }
        case QMetaType::Int: {
            const int number = value.toInt(&valid);
            const bool font = it.key() == "fontSize";
            valid = valid && number >= (font ? 8 : -1) && (!font || number <= 72);
            parsed = number;
            break;
        }
        case QMetaType::Double: {
            const double number = value.toDouble(&valid);
            valid = valid && std::isfinite(number) && number >= 0 && number <= 1;
            parsed = number;
            break;
        }
        default:
            parsed = value;
            if (it.key().endsWith("Color")) {
                static const QRegularExpression rgb(QStringLiteral("^#[0-9a-fA-F]{6}$"));
                valid = rgb.match(value).hasMatch();
            } else if (it.key().endsWith("Icon")) {
                valid = value == "mic-on" || value == "mic-muted";
            } else if (it.key() == "screen") {
                bool numeric = false;
                const int index = value.toInt(&numeric);
                valid = value == "primary" || (numeric && index >= 0);
            } else {
                valid = !value.isEmpty() && !raw.contains('\n') && !raw.contains('\r');
                parsed = raw; // Keep intentionally configured text spacing.
            }
            break;
        }
        if (valid) {
            it.value() = parsed;
        } else {
            qWarning().noquote() << "mic-overlay: invalid overlay/" + it.key()
                                << "- using default";
        }
    }
    return result;
}
