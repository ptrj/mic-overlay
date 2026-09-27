#include "pipewireevents.h"
#include <QJsonDocument>
#include <QtTest>

class PipeWireEventsTest : public QObject
{
    Q_OBJECT
    const QByteArray source = "[{\"id\":55,\"info\":{\"props\":{\"media.class\":\"Audio/Source\","
                              "\"node.description\":\"Mic [test] \\\"quoted\\\"\"}}}]";
    QByteArray metadata(const QString &sourceName, const QString &sinkName)
    {
        const QJsonObject sourceEntry{{"subject", 0}, {"key", "default.audio.source"},
                                       {"value", QJsonObject{{"name", sourceName}}}};
        const QJsonObject sinkEntry{{"subject", 0}, {"key", "default.audio.sink"},
                                     {"value", QJsonObject{{"name", sinkName}}}};
        const QJsonObject object{{"id", 39}, {"props", QJsonObject{{"metadata.name", "default"}}},
                                 {"metadata", QJsonArray{sourceEntry, sinkEntry}}};
        return QJsonDocument(QJsonArray{object}).toJson(QJsonDocument::Compact);
    }
private slots:
    void fragmentedAndConsecutiveFrames()
    {
        PipeWireEvents stream;
        const QByteArray input = " \n" + source + "\n[{\"id\":55,\"info\":null}]\n[]";
        int frames = 0;
        int relevant = 0;
        for (char byte : input) {
            const auto result = stream.feed(QByteArray(1, byte));
            QVERIFY(result.valid);
            frames += result.frame;
            relevant += result.relevant;
        }
        QCOMPARE(frames, 3);
        QCOMPARE(relevant, 2);
        QVERIFY(!stream.feed("[{\"id\":55,\"info\":{}}]").relevant);
        stream.reset();
        const auto result = stream.feed(input);
        QVERIFY(result.valid && result.frame && result.relevant);
        QVERIFY(!stream.feed("[{\"id\":55,\"info\":{}}]").relevant);
    }
    void defaultMetadataChanges()
    {
        PipeWireEvents stream;
        QVERIFY(stream.feed(metadata("mic-a", "speakers-a")).relevant);
        QVERIFY(!stream.feed(metadata("mic-a", "speakers-b")).relevant);
        QVERIFY(stream.feed(metadata("mic-b", "speakers-b")).relevant);
        QVERIFY(stream.feed("[{\"id\":39,\"metadata\":[]}]").relevant);
        QVERIFY(stream.feed("[{\"id\":39,\"info\":null}]").relevant);
        QVERIFY(!stream.feed("[{\"id\":39,\"info\":null}]").relevant);
    }
    void irrelevantClientsAndPlayback()
    {
        PipeWireEvents stream;
        QVERIFY(stream.feed(source).relevant);
        QVERIFY(!stream.feed("[{\"id\":200,\"type\":\"PipeWire:Interface:Client\",\"info\":{}}]"
                            "[{\"id\":200,\"info\":null}]"
                            "[{\"id\":201,\"info\":{\"props\":{\"media.class\":\"Audio/Sink\"}}}]").relevant);
        QVERIFY(stream.feed("[{\"id\":55,\"info\":{\"params\":{}}}]").relevant);
        stream.reset();
        QVERIFY(!stream.feed("[{\"id\":55,\"info\":{\"params\":{}}}]").relevant);
    }
    void captureDeviceChanges_data()
    {
        QTest::addColumn<QByteArray>("deviceId");
        QTest::newRow("string-id") << QByteArray("\"44\"");
        QTest::newRow("numeric-id") << QByteArray("44");
    }
    void captureDeviceChanges()
    {
        QFETCH(QByteArray, deviceId);
        PipeWireEvents stream;
        const QByteArray initial = "[{\"id\":44,\"info\":{\"props\":{\"media.class\":\"Audio/Device\"}}},"
            "{\"id\":55,\"info\":{\"props\":{\"media.class\":\"Audio/Source\",\"device.id\":"
            + deviceId + "}}}]";
        QVERIFY(stream.feed(initial).relevant);
        QVERIFY(stream.feed("[{\"id\":44,\"info\":{\"params\":{\"Route\":[{\"props\":{\"mute\":false}}]}}}]").relevant);
        QVERIFY(!stream.feed("[{\"id\":45,\"info\":{\"params\":{\"Route\":[]}}}]").relevant);
        QVERIFY(stream.feed("[{\"id\":44,\"info\":null}]").relevant);
        QVERIFY(stream.feed("[{\"id\":55,\"info\":null}]").relevant);
        QVERIFY(!stream.feed("[{\"id\":44,\"info\":{\"params\":{}}}]").relevant);
        stream.reset();
        QVERIFY(!stream.feed("[{\"id\":44,\"info\":{\"params\":{}}}]").relevant);
    }
    void captureDeviceAssociationChanges()
    {
        PipeWireEvents stream;
        QVERIFY(stream.feed("[{\"id\":55,\"info\":{\"props\":{\"media.class\":\"Audio/Source\",\"device.id\":44}}}]").relevant);
        QVERIFY(stream.feed("[{\"id\":55,\"info\":{\"props\":{\"media.class\":\"Audio/Source\",\"device.id\":45}}}]").relevant);
        QVERIFY(!stream.feed("[{\"id\":44,\"info\":{\"params\":{}}}]").relevant);
        QVERIFY(stream.feed("[{\"id\":45,\"info\":{\"params\":{}}}]").relevant);
        QVERIFY(stream.feed("[{\"id\":55,\"info\":{\"props\":{\"media.class\":\"Audio/Sink\"}}}]").relevant);
        QVERIFY(!stream.feed("[{\"id\":45,\"info\":{\"params\":{}}}]").relevant);
    }
    void invalid_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("garbage") << QByteArray("not json");
        QTest::newRow("wrong-root") << QByteArray("{}");
        QTest::newRow("syntax") << QByteArray("[{bad}]");
        QTest::newRow("wrong-entry") << QByteArray("[42]");
        QTest::newRow("missing-id") << QByteArray("[{}]");
        QTest::newRow("oversized") << QByteArray(8 * 1024 * 1024 + 1, '[');
    }
    void invalid()
    {
        QFETCH(QByteArray, bytes);
        PipeWireEvents stream;
        QVERIFY(!stream.feed(bytes).valid);
    }
};

QTEST_GUILESS_MAIN(PipeWireEventsTest)
#include "pipewireevents_test.moc"
