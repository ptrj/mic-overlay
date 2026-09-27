#include "overlaycontroller.h"
#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QtTest>
#include <memory>

class OverlayControllerTest : public QObject
{
    Q_OBJECT
    QQmlEngine m_engine;

    std::unique_ptr<QQuickWindow> window()
    {
        QQmlComponent component(&m_engine);
        component.setData(
            "import QtQuick\nimport QtQuick.Window\n"
            "Window { property bool screenReady: false; property bool overlayReady: true;"
            " visible: screenReady && overlayReady; width: 150; height: 42; color: '#26303c' }",
            QUrl());
        auto result = std::unique_ptr<QQuickWindow>(qobject_cast<QQuickWindow *>(component.create()));
        if (result) {
            result->setFlags(Qt::Tool | Qt::FramelessWindowHint
                             | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
            auto *layer = LayerShellQt::Window::get(result.get());
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)
                              | LayerShellQt::Window::AnchorLeft);
            layer->setExclusiveZone(-1);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            layer->setActivateOnShow(false);
        }
        return result;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(QGuiApplication::platformName().startsWith("wayland"));
        QGuiApplication::setQuitOnLastWindowClosed(false);
    }
    void freePositionAndCenter()
    {
        auto view = window();
        QVERIFY(view);
        auto *layer = LayerShellQt::Window::get(view.get());
        OverlayController controller(view.get(), layer, "primary");
        const auto size = view->screen()->geometry().size();
        QCOMPARE(controller.position(), QPoint((size.width() - 150) / 2, (size.height() - 42) / 2));
        controller.setPosition(321, 217);
        QCOMPARE(controller.position(), QPoint(321, 217));
        QCOMPARE(layer->margins(), QMargins(321, 217, 0, 0));
        controller.moveBy(-121, 183);
        QCOMPARE(controller.position(), QPoint(200, 400));
        for (int step = 0; step < 12; ++step) { controller.moveBy(0.25, 0.25); }
        QCOMPARE(controller.position(), QPoint(203, 403));
        controller.moveBy(100000, 100000);
        QCOMPARE(controller.position(), QPoint(size.width() - 150, size.height() - 42));
        controller.moveBy(-1, -1);
        QCOMPARE(controller.position(), QPoint(size.width() - 151, size.height() - 43));
        controller.setPosition(-1, -1);
        view->setWidth(42);
        QCOMPARE(controller.position().x(), (size.width() - 42) / 2);
        QCOMPARE(layer->margins().left(), controller.position().x());
    }
    void primaryAndNoScreens()
    {
        auto view = window();
        QVERIFY(view);
        auto *layer = LayerShellQt::Window::get(view.get());
        OverlayController controller(view.get(), layer, "primary");
        const auto screens = QGuiApplication::screens();
        auto *primary = QGuiApplication::primaryScreen();
        QCOMPARE(layer->screen(), primary);
        QVERIFY(!layer->closeOnDismissed());
        QTRY_VERIFY(view->isExposed());
        controller.reconcile({}, nullptr);
        QVERIFY(!view->isVisible());
        QVERIFY(!view->handle());
        QVERIFY(!view->property("screenReady").toBool());
        controller.reconcile(screens, primary);
        QTRY_VERIFY(view->isExposed());
        QCOMPARE(layer->screen(), primary);
    }
    void missingIndex()
    {
        auto view = window();
        QVERIFY(view);
        auto *layer = LayerShellQt::Window::get(view.get());
        OverlayController controller(view.get(), layer, "99999");
        QCOMPARE(layer->screen(), QGuiApplication::primaryScreen());
        QTRY_VERIFY(view->isExposed());
        QVERIFY(controller.m_warnedMissing);
        controller.reconcile(QGuiApplication::screens(), QGuiApplication::primaryScreen());
        QVERIFY(controller.m_warnedMissing);
    }
    void primaryChange()
    {
        const auto screens = QGuiApplication::screens();
        if (screens.size() < 2) {
            QSKIP("Requires two real outputs");
        }
        auto view = window();
        QVERIFY(view);
        auto *layer = LayerShellQt::Window::get(view.get());
        OverlayController controller(view.get(), layer, "primary");
        auto *other = screens.at(0) == QGuiApplication::primaryScreen() ? screens.at(1) : screens.at(0);
        controller.reconcile(screens, other);
        QTRY_VERIFY(view->isExposed());
        QCOMPARE(view->screen(), other);
        QCOMPARE(layer->screen(), other);
        QCOMPARE(QGuiApplication::allWindows().size(), 1);
    }
    void removalReturnAndHiddenState()
    {
        const auto screens = QGuiApplication::screens();
        if (screens.size() < 2) {
            QSKIP("Requires two real outputs");
        }
        auto view = window();
        QVERIFY(view);
        auto *layer = LayerShellQt::Window::get(view.get());
        OverlayController controller(view.get(), layer, "1");
        QTRY_VERIFY(view->isExposed());
        QCOMPARE(layer->screen(), screens.at(1));
        controller.reconcile({screens.at(0)}, screens.at(0));
        QTRY_VERIFY(view->isExposed());
        QCOMPARE(layer->screen(), screens.at(0));
        controller.reconcile(screens, screens.at(0));
        QTRY_VERIFY(view->isExposed());
        QCOMPARE(layer->screen(), screens.at(1));
        QVERIFY(!controller.m_warnedMissing);
        view->setProperty("overlayReady", false);
        controller.reconcile({screens.at(0)}, screens.at(0));
        QVERIFY(!view->isVisible());
        controller.reconcile(screens, screens.at(0));
        QVERIFY(!view->isVisible());
        // Moving monitors must preserve the QML visibility binding.
        view->setProperty("overlayReady", true);
        QTRY_VERIFY(view->isExposed());
        QCOMPARE(layer->screen(), screens.at(1));
        QCOMPARE(QGuiApplication::allWindows().size(), 1);
    }
};

QTEST_MAIN(OverlayControllerTest)
#include "overlaycontroller_test.moc"
