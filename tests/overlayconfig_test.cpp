#include "overlayconfig.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class OverlayConfigTest : public QObject
{
    Q_OBJECT
private slots:
    void missingFile()
    {
        QTemporaryDir dir;
        QCOMPARE(OverlayConfig::load(dir.filePath("missing.ini")), OverlayConfig::defaults());
    }
    void validOverrides()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("config.ini"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[overlay]\nx=5311\ny=312\n"
                   "showWhenUnmuted=false\nhideWhenUnavailable=0\nopacity=0.4\n"
                   "fontSize=22\nbold=false\nmutedText=STLMENE\nunmutedText=ZAPNUTE\n"
                   "mutedIcon=mic-on\nunmutedIcon=mic-muted\n"
                   "mutedColor=#990022\nunmutedColor=#008844\n");
        file.close();
        const auto config = OverlayConfig::load(file.fileName());
        QCOMPARE(config.value("x").toInt(), 5311);
        QCOMPARE(config.value("y").toInt(), 312);
        QVERIFY(!config.value("showWhenUnmuted").toBool());
        QVERIFY(!config.value("hideWhenUnavailable").toBool());
        QCOMPARE(config.value("opacity").toDouble(), 0.4);
        QCOMPARE(config.value("fontSize").toInt(), 22);
        QVERIFY(!config.value("bold").toBool());
        QCOMPARE(config.value("mutedText").toString(), "STLMENE");
        QCOMPARE(config.value("unmutedText").toString(), "ZAPNUTE");
        QCOMPARE(config.value("mutedIcon").toString(), "mic-on");
        QCOMPARE(config.value("unmutedIcon").toString(), "mic-muted");
        QCOMPARE(config.value("mutedColor").toString(), "#990022");
        QCOMPARE(config.value("unmutedColor").toString(), "#008844");
    }
    void unquotedTextWithComma()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("config.ini"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[overlay]\nmutedText=MUTED, ON AIR\nmutedColor=#990022, #008844\n");
        file.close();
        const auto config = OverlayConfig::load(file.fileName());
        QCOMPARE(config.value("mutedText").toString(), "MUTED, ON AIR");
        QCOMPARE(config.value("mutedColor").toString(), "#7b202b");
    }
    void savePositionRemovesLegacyKeys()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("config.ini");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[overlay]\nposition=top-right\nmarginTop=24\nopacity=0.35\n");
        file.close();
        auto config = OverlayConfig::load(path);
        QCOMPARE(config.value("opacity").toDouble(), 0.35);
        QCOMPARE(config.value("x").toInt(), -1);
        config["x"] = 5210;
        config["y"] = 450;
        QString error;
        QVERIFY(OverlayConfig::save(path, config, &error));
        QCOMPARE(OverlayConfig::load(path), config);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto contents = file.readAll();
        QVERIFY(!contents.contains("position="));
        QVERIFY(!contents.contains("marginTop="));
    }
    void invalidOverrides()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("config.ini"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[overlay]\nx=-2\ny=invalid\n"
                   "fontSize=abc\nopacity=nan\nshowWhenUnmuted=maybe\n"
                   "hideWhenUnavailable=off\nbold=perhaps\nmutedText=\n"
                   "mutedIcon=/etc/passwd\nscreen=-1\n"
                   "mutedColor=#12345\nunmutedColor=invalid\n");
        file.close();
        QCOMPARE(OverlayConfig::load(file.fileName()), OverlayConfig::defaults());
    }
};

QTEST_GUILESS_MAIN(OverlayConfigTest)
#include "overlayconfig_test.moc"
