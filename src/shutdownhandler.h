#pragma once

#include <QSocketNotifier>
#include <csignal>
#include <memory>

class QCoreApplication;

// Convert Unix termination signals into a Qt quit event so QProcess cleanup runs.
class ShutdownHandler
{
public:
    bool install(QCoreApplication &app);
    ~ShutdownHandler();

private:
    int m_pipe[2] = {-1, -1};
    std::unique_ptr<QSocketNotifier> m_notifier;
    struct sigaction m_oldInt {};
    struct sigaction m_oldTerm {};
    bool m_intInstalled = false;
    bool m_termInstalled = false;
};
