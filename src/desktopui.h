#pragma once

#include <QObject>
#include <QVariantMap>
#include <memory>

class MicStateMonitor;
class QDialog;
class QMenu;
class QSystemTrayIcon;

class DesktopUi : public QObject
{
    Q_OBJECT
public:
    DesktopUi(MicStateMonitor *monitor, const QString &configPath, QObject *parent = nullptr);
    ~DesktopUi() override;
    void showConfigure();
    void showError(const QString &message);

signals:
    void configurationChanged(const QVariantMap &config);
    void previewChanged(const QString &key, const QVariant &value);
    void editingChanged(bool editing);
    void resetPositionRequested();

private:
    void updateStatus();
    MicStateMonitor *m_monitor;
    QString m_configPath;
    std::unique_ptr<QMenu> m_menu;
    std::unique_ptr<QSystemTrayIcon> m_tray;
    std::unique_ptr<QDialog> m_dialog;
};
