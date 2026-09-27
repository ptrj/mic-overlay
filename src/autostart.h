#pragma once
#include <QString>
namespace Autostart {
QString path();
bool enabled(const QString &file = path());
QString desktopEntry(const QString &executable, const QString &configuration);
bool setEnabled(bool enable, const QString &executable, const QString &configuration,
                QString *error, const QString &file = path());
}
