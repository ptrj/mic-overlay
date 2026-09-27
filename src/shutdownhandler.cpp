#include "shutdownhandler.h"

#include <QCoreApplication>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace {
volatile sig_atomic_t signalPipe = -1;
void terminateApplication(int)
{
    const int savedErrno = errno;
    if (signalPipe >= 0) {
        const char byte = 1;
        const auto ignored = ::write(signalPipe, &byte, 1);
        (void)ignored;
    }
    errno = savedErrno;
}
}

bool ShutdownHandler::install(QCoreApplication &app)
{
    if (::pipe2(m_pipe, O_CLOEXEC | O_NONBLOCK) != 0) {
        return false;
    }
    signalPipe = m_pipe[1];
    struct sigaction action {};
    action.sa_handler = terminateApplication;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    m_intInstalled = ::sigaction(SIGINT, &action, &m_oldInt) == 0;
    m_termInstalled = ::sigaction(SIGTERM, &action, &m_oldTerm) == 0;
    if (!m_intInstalled || !m_termInstalled) {
        return false;
    }
    m_notifier = std::make_unique<QSocketNotifier>(m_pipe[0], QSocketNotifier::Read);
    QObject::connect(m_notifier.get(), &QSocketNotifier::activated, &app, [this, &app] {
        char bytes[64];
        while (::read(m_pipe[0], bytes, sizeof(bytes)) > 0) {}
        app.quit();
    });
    return true;
}

ShutdownHandler::~ShutdownHandler()
{
    m_notifier.reset();
    if (m_intInstalled) { ::sigaction(SIGINT, &m_oldInt, nullptr); }
    if (m_termInstalled) { ::sigaction(SIGTERM, &m_oldTerm, nullptr); }
    signalPipe = -1;
    for (int fd : m_pipe) {
        if (fd >= 0) { ::close(fd); }
    }
}
