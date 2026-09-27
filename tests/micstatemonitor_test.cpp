#include "micstatemonitor.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <csignal>
#include <QtTest>

class MicStateMonitorTest : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_dir;
    QString program() const { return m_dir.filePath(QStringLiteral("wpctl")); }
    QString eventsProgram() const { return m_dir.filePath(QStringLiteral("pw-dump")); }
    void write(const QString &name, const QByteArray &data, bool append = false)
    {
        QFile file(m_dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly | (append ? QIODevice::Append : QIODevice::Truncate)));
        QCOMPARE(file.write(data), data.size());
    }
    QByteArray read(const QString &name)
    {
        QFile file(m_dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) { return {}; }
        return file.readAll();
    }
    void prepareEvents(const QByteArray &mode)
    {
        write("monitor-mode", mode);
        write("monitor-starts", "");
        write("events", "[{\"id\":55,\"type\":\"PipeWire:Interface:Node\","
                        "\"info\":{\"props\":{\"media.class\":\"Audio/Source\"}}}]\n");
    }
    void state(const QByteArray &value)
    {
        QFile file(m_dir.filePath(QStringLiteral("state")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(value), value.size());
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(QFile::copy(QStringLiteral(QT_TESTCASE_SOURCEDIR "/tests/fake-wpctl.sh"),
                            program()));
        QFile file(program());
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(QFile::copy(QStringLiteral(QT_TESTCASE_SOURCEDIR "/tests/fake-pw-dump.sh"),
                            eventsProgram()));
        QFile events(eventsProgram());
        QVERIFY(events.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
    void startup_data()
    {
        QTest::addColumn<QByteArray>("value");
        QTest::addColumn<bool>("muted");
        QTest::newRow("muted") << QByteArray("muted") << true;
        QTest::newRow("unmuted") << QByteArray("unmuted") << false;
    }
    void startup()
    {
        QFETCH(QByteArray, value);
        QFETCH(bool, muted);
        state(value);
        MicStateMonitor monitor(nullptr, program(), 2000, QString());
        QVERIFY(!monitor.available());
        monitor.start();
        // Completion before the first 500 ms timer proves the immediate query.
        QTRY_VERIFY_WITH_TIMEOUT(monitor.available(), 300);
        QCOMPARE(monitor.muted(), muted);
    }
    void pollingAndRecovery()
    {
        state("unmuted");
        MicStateMonitor monitor(nullptr, program(), 2000, QString());
        QSignalSpy muteChanges(&monitor, &MicStateMonitor::mutedChanged);
        monitor.start();
        QTRY_VERIFY(monitor.available());
        state("muted");
        QTRY_VERIFY_WITH_TIMEOUT(monitor.muted(), 1000);
        QCOMPARE(muteChanges.count(), 1);
        QTest::qWait(600);
        QCOMPARE(muteChanges.count(), 1);
        state("error");
        QTRY_VERIFY_WITH_TIMEOUT(!monitor.available(), 1000);
        state("unmuted");
        QTRY_VERIFY_WITH_TIMEOUT(monitor.available() && !monitor.muted(), 1000);
        QCOMPARE(muteChanges.count(), 2);
    }
    void invalidOutput_data()
    {
        QTest::addColumn<QByteArray>("value");
        QTest::newRow("nonzero-exit-with-valid-stdout") << QByteArray("error");
        QTest::newRow("empty") << QByteArray("empty");
        QTest::newRow("malformed") << QByteArray("malformed");
    }
    void invalidOutput()
    {
        QFETCH(QByteArray, value);
        state("muted");
        MicStateMonitor monitor(nullptr, program(), 2000, QString());
        monitor.refresh();
        QTRY_VERIFY(monitor.available());
        state(value);
        monitor.refresh();
        QTRY_VERIFY(!monitor.available());
    }
    void timeoutAndNoOverlap()
    {
        state("muted");
        MicStateMonitor monitor(nullptr, program(), 250, QString());
        monitor.start();
        QTRY_VERIFY(monitor.available());
        state("slow");
        monitor.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(m_dir.filePath("slow-started")), 150);
        state("unmuted");
        monitor.refresh();
        monitor.refresh();
        // These requests must not run while the slow query is active.
        QTest::qWait(50);
        QVERIFY(monitor.available());
        QVERIFY(monitor.muted());
        QTRY_VERIFY_WITH_TIMEOUT(!monitor.available(), 500);
        monitor.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(monitor.available(), 1000);
        QVERIFY(!monitor.muted());
    }
    void missingExecutable()
    {
        MicStateMonitor monitor(nullptr, m_dir.filePath(QStringLiteral("missing")), 2000, QString());
        monitor.start();
        QTest::qWait(600);
        QVERIFY(!monitor.available());
    }
    void eventDrivenIdleFilteringAndBatching()
    {
        state("unmuted");
        prepareEvents("valid");
        MicStateMonitor monitor(nullptr, program(), 2000, eventsProgram());
        monitor.start();
        QTRY_VERIFY(monitor.available() && monitor.eventDriven());
        QTest::qWait(450);
        const int queries = read("queries").count('\n');
        QTest::qWait(1100);
        QCOMPARE(read("queries").count('\n'), queries);
        write("events", "[{\"id\":200,\"type\":\"PipeWire:Interface:Client\",\"info\":{}}]\n"
                        "[{\"id\":200,\"info\":null}]\n", true);
        QTest::qWait(1400);
        QCOMPARE(read("queries").count('\n'), queries);
        state("muted");
        const QByteArray event = "[{\"id\":55,\"info\":{\"params\":{\"Props\":[{\"mute\":true}]}}}]\n";
        write("events", event.repeated(20), true);
        QTRY_VERIFY_WITH_TIMEOUT(monitor.muted(), 2000);
        QTest::qWait(350);
        QCOMPARE(read("queries").count('\n'), queries + 1);
    }
    void mutedStartupWithDeviceRouteChange()
    {
        state("muted");
        prepareEvents("valid");
        write("events", "[{\"id\":44,\"type\":\"PipeWire:Interface:Device\","
                        "\"info\":{\"props\":{\"media.class\":\"Audio/Device\"}}},"
                        "{\"id\":55,\"type\":\"PipeWire:Interface:Node\","
                        "\"info\":{\"props\":{\"media.class\":\"Audio/Source\",\"device.id\":\"44\"}}}]\n");
        MicStateMonitor monitor(nullptr, program(), 2000, eventsProgram());
        monitor.start();
        QTRY_VERIFY(monitor.available() && monitor.eventDriven() && monitor.muted());
        QTest::qWait(450);
        state("unmuted");
        write("events", "[{\"id\":44,\"type\":\"PipeWire:Interface:Device\","
                        "\"info\":{\"params\":{\"Route\":[{\"props\":{\"mute\":false}}]}}}]\n", true);
        QTRY_VERIFY_WITH_TIMEOUT(!monitor.muted(), 2000);
        state("muted");
        write("events", "[{\"id\":44,\"info\":{\"params\":{\"Route\":[{\"props\":{\"mute\":true}}]}}}]\n", true);
        QTRY_VERIFY_WITH_TIMEOUT(monitor.muted(), 2000);
    }
    void failedMonitorRecovery()
    {
        state("unmuted");
        prepareEvents("fail");
        MicStateMonitor monitor(nullptr, program(), 2000, eventsProgram(), 600);
        monitor.start();
        QTRY_VERIFY(monitor.available());
        QVERIFY(!monitor.eventDriven());
        state("muted");
        QTRY_VERIFY_WITH_TIMEOUT(monitor.muted(), 1000);
        write("monitor-mode", "valid");
        QTRY_VERIFY_WITH_TIMEOUT(monitor.eventDriven(), 1500);
        QTest::qWait(350);
        const int queries = read("queries").count('\n');
        QTest::qWait(700);
        QCOMPARE(read("queries").count('\n'), queries);
    }
    void eventDuringActiveQuery()
    {
        state("unmuted");
        prepareEvents("valid");
        MicStateMonitor monitor(nullptr, program(), 2000, eventsProgram());
        monitor.start();
        QTRY_VERIFY(monitor.available() && monitor.eventDriven());
        QTest::qWait(450);
        state("delayed");
        monitor.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(m_dir.filePath("delayed-started")), 300);
        state("muted");
        write("events", "[{\"id\":55,\"info\":{\"params\":{}}}]\n", true);
        QTRY_VERIFY_WITH_TIMEOUT(monitor.muted(), 3000);
        QVERIFY(monitor.eventDriven());
    }
    void missingMonitorExecutableRecovery()
    {
        state("unmuted");
        prepareEvents("valid");
        const auto newProgram = m_dir.filePath("new-pw-dump");
        MicStateMonitor monitor(nullptr, program(), 2000, newProgram, 600);
        monitor.start();
        QTRY_VERIFY(monitor.available());
        QVERIFY(!monitor.eventDriven());
        state("muted");
        QTRY_VERIFY_WITH_TIMEOUT(monitor.muted(), 1000);
        QVERIFY(QFile::copy(eventsProgram(), newProgram));
        QTRY_VERIFY_WITH_TIMEOUT(monitor.eventDriven(), 1500);
    }
    void runningMonitorCrashRecovery()
    {
        state("muted");
        prepareEvents("valid");
        MicStateMonitor monitor(nullptr, program(), 2000, eventsProgram(), 700);
        monitor.start();
        QTRY_VERIFY(monitor.available() && monitor.eventDriven());
        const int pid = read("monitor-pid").toInt();
        QVERIFY(pid > 1);
        write("monitor-mode", "fail");
        QVERIFY(::kill(pid, SIGKILL) == 0);
        QTRY_VERIFY(!monitor.eventDriven());
        state("unmuted");
        QTRY_VERIFY_WITH_TIMEOUT(!monitor.muted(), 1000);
        write("monitor-mode", "valid");
        QTRY_VERIFY_WITH_TIMEOUT(monitor.eventDriven(), 1800);
    }
    void unavailableProbeWithHealthyMonitor()
    {
        state("error");
        prepareEvents("valid");
        MicStateMonitor monitor(nullptr, program(), 2000, eventsProgram());
        monitor.start();
        QTRY_VERIFY(monitor.eventDriven());
        QVERIFY(!monitor.available());
        // No event is sent: recovery probes must bridge WirePlumber startup gaps.
        state("muted");
        QTRY_VERIFY_WITH_TIMEOUT(monitor.available() && monitor.muted(), 1000);
    }
    void invalidOrSilentMonitor_data()
    {
        QTest::addColumn<QByteArray>("mode");
        QTest::newRow("malformed-stream") << QByteArray("invalid");
        QTest::newRow("no-initial-frame") << QByteArray("silent");
    }
    void invalidOrSilentMonitor()
    {
        QFETCH(QByteArray, mode);
        state("unmuted");
        prepareEvents(mode);
        MicStateMonitor monitor(nullptr, program(), 2000, eventsProgram(), 600);
        monitor.start();
        QTRY_VERIFY(monitor.available());
        QVERIFY(!monitor.eventDriven());
        state("muted");
        QTRY_VERIFY_WITH_TIMEOUT(monitor.muted(), 1000);
        // Silent initialization is killed at 2 seconds and retried afterwards.
        QTRY_VERIFY_WITH_TIMEOUT(read("monitor-starts").count('\n') >= 2, 3500);
        write("monitor-mode", "valid");
        QTRY_VERIFY_WITH_TIMEOUT(monitor.eventDriven(), 3500);
    }
};

QTEST_GUILESS_MAIN(MicStateMonitorTest)
#include "micstatemonitor_test.moc"
