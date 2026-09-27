#include "micstatemonitor.h"
#include "shutdownhandler.h"
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ShutdownHandler shutdown;
    if (!shutdown.install(app)) { return 1; }
    MicStateMonitor monitor;
    monitor.start();
    QTimer::singleShot(45000, &app, &QCoreApplication::quit);
    return app.exec();
}
