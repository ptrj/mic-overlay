#include "micstatemonitor.h"

#include <QDebug>
#include <QRegularExpression>

MicStateMonitor::MicStateMonitor(QObject *parent, const QString &program, int timeoutMs,
                               const QString &eventProgram, int reconnectMs)
    : QObject(parent)
{
    m_process.setProgram(program);
    m_process.setArguments({QStringLiteral("get-volume"),
                            QStringLiteral("@DEFAULT_AUDIO_SOURCE@")});
    // stderr is diagnostic only; it must never be interpreted as microphone state.
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_poll.setInterval(500);
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(timeoutMs);
    m_eventProcess.setProgram(eventProgram);
    m_eventProcess.setArguments({QStringLiteral("--monitor"), QStringLiteral("--no-colors")});
    m_eventDeadline.setSingleShot(true);
    m_eventDeadline.setInterval(2000);
    m_reconnect.setSingleShot(true);
    m_reconnect.setInterval(reconnectMs);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(200);
    connect(&m_debounce, &QTimer::timeout, this, &MicStateMonitor::refresh);
    connect(&m_reconnect, &QTimer::timeout, this, &MicStateMonitor::startEventMonitor);
    connect(&m_eventDeadline, &QTimer::timeout, this, [this] {
        eventMonitorLost(QStringLiteral("PipeWire event monitor initialization timed out"));
    });
    connect(&m_eventProcess, &QProcess::readyReadStandardError, this, [this] {
        m_eventProcess.readAllStandardError();
    });
    connect(&m_eventProcess, &QProcess::readyReadStandardOutput, this, [this] {
        const QByteArray bytes = m_eventProcess.readAllStandardOutput();
        if (m_eventBad) {
            return;
        }
        const auto result = m_events.feed(bytes);
        if (!result.valid) {
            eventMonitorLost(QStringLiteral("invalid PipeWire event stream"));
            return;
        }
        const bool firstFrame = result.frame && !m_eventDriven;
        if (result.frame) {
            m_eventDeadline.stop();
            m_eventDriven = true;
            m_warnedMonitor = false;
            updatePolling();
            if (firstFrame) {
                qInfo("mic-overlay: PipeWire event monitoring active");
            }
        }
        if (firstFrame || result.relevant) {
            scheduleRefresh();
        }
    });
    connect(&m_eventProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            eventMonitorLost(QStringLiteral("could not start PipeWire event monitor"));
        }
    });
    connect(&m_eventProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this] { eventMonitorLost(QStringLiteral("PipeWire event monitor stopped")); });
    connect(&m_poll, &QTimer::timeout, this, &MicStateMonitor::refresh);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_queryFailed = true;
        fail(QStringLiteral("wpctl query timed out"));
        m_process.kill();
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_process.readAllStandardError();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output += m_process.readAllStandardOutput();
        if (m_output.size() > 4096) {
            m_output.clear();
            m_queryFailed = true;
            fail(QStringLiteral("wpctl output exceeded limit"));
            m_process.kill();
        }
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_deadline.stop();
            m_queryFailed = true;
            fail(QStringLiteral("could not start wpctl"));
        }
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        m_deadline.stop();
        if (m_queryFailed) {
            return;
        }
        if (status != QProcess::NormalExit || exitCode != 0) {
            fail(QStringLiteral("wpctl query failed (no source or audio service unavailable)"));
            return;
        }
        // Validate the shape, but do not parse or use the volume value.
        static const QRegularExpression validOutput(
            QStringLiteral(R"(^Volume:\s+\d+(?:\.\d+)?(?:\s+\[MUTED\])?$)"));
        const QString output = QString::fromUtf8(m_output).trimmed();
        if (!validOutput.match(output).hasMatch()) {
            fail(QStringLiteral("invalid wpctl output"));
            return;
        }
        m_lastFailure.clear();
        setState(true, output.contains(QStringLiteral("[MUTED]")));
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this] {
        if (m_refreshPending) {
            m_refreshPending = false;
            scheduleRefresh();
        }
    });
}

MicStateMonitor::~MicStateMonitor()
{
    // Shutdown cancellation is not an audio failure and must not change UI state.
    disconnect(&m_process, nullptr, this, nullptr);
    disconnect(&m_eventProcess, nullptr, this, nullptr);
    m_poll.stop();
    m_deadline.stop();
    m_eventDeadline.stop();
    m_reconnect.stop();
    m_debounce.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    if (m_eventProcess.state() != QProcess::NotRunning) {
        m_eventProcess.kill();
        m_eventProcess.waitForFinished(1000);
    }
}

void MicStateMonitor::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    refresh();
    updatePolling();
    startEventMonitor();
}

void MicStateMonitor::refresh()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_refreshPending = true;
        return;
    }
    m_output.clear();
    m_queryFailed = false;
    m_deadline.start();
    m_process.start();
}

void MicStateMonitor::fail(const QString &reason)
{
    if (m_lastFailure != reason) {
        qWarning().noquote() << "mic-overlay:" << reason;
        m_lastFailure = reason;
    }
    setState(false);
}

void MicStateMonitor::setState(bool available, bool muted)
{
    // Retain the last known mute value during outages; available is authoritative.
    const bool muteChanged = available && (muted != m_muted || !m_available);
    if (available && muted != m_muted) {
        m_muted = muted;
        emit mutedChanged();
    }
    if (available != m_available) {
        m_available = available;
        emit availableChanged();
        qInfo() << "mic-overlay: default microphone"
                << (available ? "available" : "unavailable");
    }
    if (muteChanged) {
        qInfo() << "mic-overlay: state changed:" << (muted ? "muted" : "unmuted");
    }
    updatePolling();
}

void MicStateMonitor::updatePolling()
{
    // Unavailable state needs probing even when PipeWire itself remains running
    // (for example WirePlumber is still starting and no source is resolved yet).
    if (m_started && (!m_eventDriven || !m_available)) {
        if (!m_poll.isActive()) {
            m_poll.start();
        }
    } else {
        m_poll.stop();
    }
}

void MicStateMonitor::scheduleRefresh()
{
    // Bounded batching: do not postpone indefinitely during a continuous burst.
    if (!m_debounce.isActive()) {
        m_debounce.start();
    }
}

void MicStateMonitor::startEventMonitor()
{
    if (!m_started || m_eventProcess.program().isEmpty()
        || m_eventProcess.state() != QProcess::NotRunning) {
        return;
    }
    m_events.reset();
    m_eventBad = false;
    m_eventDeadline.start();
    m_eventProcess.start();
}

void MicStateMonitor::eventMonitorLost(const QString &reason)
{
    m_eventDeadline.stop();
    m_eventDriven = false;
    m_eventBad = true;
    if (!m_warnedMonitor) {
        qWarning().noquote() << "mic-overlay:" << reason << "- using fallback polling";
        m_warnedMonitor = true;
    }
    updatePolling();
    scheduleRefresh();
    if (m_eventProcess.state() != QProcess::NotRunning) {
        m_eventProcess.kill();
    }
    if (!m_reconnect.isActive()) {
        m_reconnect.start();
    }
}
