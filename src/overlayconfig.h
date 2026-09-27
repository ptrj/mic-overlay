#pragma once

#include <QString>
#include <QVariantMap>

namespace OverlayConfig
{
QString defaultPath();
QVariantMap defaults();
QVariantMap load(const QString &path);
bool save(const QString &path, const QVariantMap &values, QString *error);
}
