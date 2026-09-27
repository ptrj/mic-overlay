#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include "pipewireevents.h"

class MicStateMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    explicit MicStateMonitor(QObject *parent = nullptr,
                             const QString &program = QStringLiteral("wpctl"),
                             int timeoutMs = 2000,
                             const QString &eventProgram = QStringLiteral("pw-dump"),
                             int reconnectMs = 3000);
    ~MicStateMonitor() override;
    bool muted() const { return m_muted; }
    bool available() const { return m_available; }
    bool eventDriven() const { return m_eventDriven; }
    void start();
    void refresh();

signals:
    void mutedChanged();
    void availableChanged();

private:
    void setState(bool available, bool muted = false);
    void fail(const QString &reason);
    void startEventMonitor();
    void eventMonitorLost(const QString &reason);
    void scheduleRefresh();
    void updatePolling();
    QProcess m_process;
    QProcess m_eventProcess;
    QTimer m_poll;
    QTimer m_deadline;
    QTimer m_eventDeadline;
    QTimer m_reconnect;
    QTimer m_debounce;
    PipeWireEvents m_events;
    QByteArray m_output;
    QString m_lastFailure;
    bool m_muted = false;
    bool m_available = false;
    bool m_queryFailed = false;
    bool m_started = false;
    bool m_eventDriven = false;
    bool m_eventBad = false;
    bool m_warnedMonitor = false;
    bool m_refreshPending = false;
};
