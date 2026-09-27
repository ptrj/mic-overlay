#include "autostart.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

QString Autostart::path()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/mic-overlay.desktop");
}

bool Autostart::enabled(const QString &file)
{
    if (!QFileInfo::exists(file)) { return false; }
    QSettings entry(file, QSettings::IniFormat);
    return !entry.value(QStringLiteral("Desktop Entry/Hidden"), false).toBool();
}

static QString quote(QString argument)
{
    argument.replace('\\', QStringLiteral("\\\\"));
    argument.replace('"', QStringLiteral("\\\""));
    argument.replace('$', QStringLiteral("\\$"));
    argument.replace('`', QStringLiteral("\\`"));
    argument.replace('%', QStringLiteral("%%"));
    // Desktop Entry string escaping precedes Exec argument parsing.
    argument.replace('\\', QStringLiteral("\\\\"));
    return '"' + argument + '"';
}

QString Autostart::desktopEntry(const QString &executable, const QString &configuration)
{
    return QStringLiteral("[Desktop Entry]\nType=Application\nName=Microphone Overlay\n"
                          "Comment=Microphone status overlay\nExec=%1 --config %2\n"
                          "Icon=mic-overlay\nTerminal=false\nOnlyShowIn=KDE;\n")
        .arg(quote(executable), quote(configuration));
}

bool Autostart::setEnabled(bool enable, const QString &executable,
                          const QString &configuration, QString *error, const QString &file)
{
    if (!enable) {
        if (!QFileInfo::exists(file) || QFile::remove(file)) { return true; }
        *error = QStringLiteral("Could not remove autostart entry: ") + file;
        return false;
    }
    if (executable.contains('\n') || executable.contains('\r')
        || configuration.contains('\n') || configuration.contains('\r')) {
        *error = QStringLiteral("Autostart paths cannot contain line breaks.");
        return false;
    }
    if (!QDir().mkpath(QFileInfo(file).absolutePath())) {
        *error = QStringLiteral("Could not create the autostart directory.");
        return false;
    }
    QSaveFile output(file);
    const QByteArray contents = desktopEntry(executable, configuration).toUtf8();
    if (!output.open(QIODevice::WriteOnly) || output.write(contents) != contents.size()
        || !output.commit()) {
        *error = QStringLiteral("Could not save autostart entry: ") + output.errorString();
        return false;
    }
    return true;
}
