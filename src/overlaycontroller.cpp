#include "overlaycontroller.h"

#include <LayerShellQt/Window>
#include <QDebug>
#include <QGuiApplication>
#include <QRegion>
#include <QtMath>

namespace {
QList<QScreen *> availableScreens()
{
    auto screens = QGuiApplication::screens();
    // Qt uses an unnamed placeholder when no Wayland outputs remain.
    screens.removeIf([](QScreen *screen) { return screen->name().isEmpty(); });
    return screens;
}
}

void OverlayController::applyConfiguration(const QVariantMap &values)
{
    m_window->setProperty("overlayReady", false);
    setPosition(values.value("x").toInt(), values.value("y").toInt());
    m_window->setProperty("overlayConfig", values);
    setScreenSelection(values.value("screen").toString());
    m_window->setProperty("overlayReady", true);
}

void OverlayController::previewSetting(const QString &key, const QVariant &value)
{
    auto values = m_window->property("overlayConfig").toMap();
    values[key] = value;
    if (key == "screen") {
        setScreenSelection(value.toString());
    }
    m_window->setProperty("overlayConfig", values);
}

void OverlayController::setEditing(bool editing)
{
    m_window->setProperty("overlayReady", false);
    // Recreate the Wayland surface to refresh its input region and size.
    m_window->destroy();
    m_window->setFlag(Qt::WindowTransparentForInput, !editing);
    m_window->setProperty("editing", editing);
    applyPosition();
    m_window->setProperty("overlayReady", true);
}

void OverlayController::moveOverlay(double dx, double dy)
{
    moveBy(dx, dy);
    auto values = m_window->property("overlayConfig").toMap();
    const auto point = position();
    values["x"] = point.x();
    values["y"] = point.y();
    m_window->setProperty("overlayConfig", values);
}

void OverlayController::resetPosition()
{
    setPosition(-1, -1);
    auto values = m_window->property("overlayConfig").toMap();
    values["x"] = -1;
    values["y"] = -1;
    m_window->setProperty("overlayConfig", values);
    emit positionSaveRequested();
}

QSize OverlayController::indicatorSize() const
{
    const auto width = m_window->property("indicatorWidth");
    const auto height = m_window->property("indicatorHeight");
    return {width.isValid() ? qCeil(width.toDouble()) : m_window->width(),
            height.isValid() ? qCeil(height.toDouble()) : m_window->height()};
}

void OverlayController::setPosition(int x, int y)
{
    m_x = x;
    m_y = y;
    m_fraction = {};
    applyPosition();
}

QPoint OverlayController::position() const
{
    if (!m_window || !m_selected) { return {}; }
    const QSize size = m_selected->geometry().size();
    const QSize indicator = indicatorSize();
    const int right = qMax(0, size.width() - indicator.width());
    const int bottom = qMax(0, size.height() - indicator.height());
    return {m_x < 0 ? right / 2 : qBound(0, m_x, right),
            m_y < 0 ? bottom / 2 : qBound(0, m_y, bottom)};
}

void OverlayController::applyPosition()
{
    if (!m_window || !m_selected) { return; }
    const auto point = position();
    const auto size = m_selected->geometry().size();
    m_window->setProperty("monitorWidth", size.width());
    m_window->setProperty("monitorHeight", size.height());
    if (m_window->property("editing").toBool()) {
        m_window->setProperty("overlayX", point.x());
        m_window->setProperty("overlayY", point.y());
        m_layer->setMargins(QMargins());
        // Only the indicator receives input. The remaining transparent canvas
        // stays click-through, including over the Configure window.
        m_window->setMask(QRegion(QRect(point, indicatorSize())));
    } else {
        m_window->setMask(QRegion());
        m_layer->setMargins(QMargins(point.x(), point.y(), 0, 0));
    }
}

void OverlayController::moveBy(double dx, double dy)
{
    const auto point = position();
    const QPointF movement = m_fraction + QPointF(dx, dy);
    const QPoint requested(qMax(0, point.x() + qRound(movement.x())),
                           qMax(0, point.y() + qRound(movement.y())));
    setPosition(requested.x(), requested.y());
    // Preserve subpixel input under fractional scaling. Discard overshoot at
    // the monitor edges so reversing direction responds immediately.
    const auto bounded = position();
    m_fraction = QPointF(
        bounded.x() == requested.x() && requested.x() > 0 ? movement.x() - qRound(movement.x()) : 0,
        bounded.y() == requested.y() && requested.y() > 0 ? movement.y() - qRound(movement.y()) : 0);
    m_x = bounded.x();
    m_y = bounded.y();
}

void OverlayController::setScreenSelection(const QString &selection)
{
    m_selection = selection;
    m_warnedMissing = false;
    m_refresh.start();
}

OverlayController::OverlayController(QWindow *window, LayerShellQt::Window *layer,
                                     const QString &screenSelection)
    : QObject(window), m_window(window), m_layer(layer), m_selection(screenSelection)
{
    const auto resized = [this] {
        if (!m_window->property("editing").toBool()) { applyPosition(); }
    };
    connect(window, &QWindow::widthChanged, this, resized);
    connect(window, &QWindow::heightChanged, this, resized);
    // A removed output may dismiss its surface. Keep the application alive so
    // that the same QWindow can be mapped on a surviving/new output.
    m_layer->setCloseOnDismissed(false);
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(0);
    connect(&m_refresh, &QTimer::timeout, this, [this] {
        reconcile(availableScreens(), QGuiApplication::primaryScreen());
    });
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this] { m_refresh.start(); });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this] { m_refresh.start(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
        if (m_selected == screen) {
            suspend();
            m_selected.clear();
        }
        // Defer until Qt has finished removing the screen and updating primary.
        m_refresh.start();
    });
    reconcile(availableScreens(), QGuiApplication::primaryScreen());
}

void OverlayController::suspend()
{
    if (!m_window) {
        return;
    }
    m_window->setProperty("screenReady", false);
    // An output is selected only when creating a layer surface. Recreate its
    // platform surface to move it, retaining the QML object and visibility binding.
    m_window->destroy();
    m_layer->setScreen(nullptr);
}

void OverlayController::reconcile(const QList<QScreen *> &screens, QScreen *primary)
{
    if (!m_window) {
        return;
    }
    if (screens.isEmpty()) {
        suspend();
        m_selected.clear();
        return;
    }
    QScreen *target = screens.contains(primary) ? primary : screens.first();
    if (m_selection != QStringLiteral("primary")) {
        bool valid = false;
        const int index = m_selection.toInt(&valid);
        if (valid && index >= 0 && index < screens.size()) {
            target = screens.at(index);
            m_warnedMissing = false;
        } else if (!m_warnedMissing) {
            qWarning("mic-overlay: selected screen index unavailable; using primary");
            m_warnedMissing = true;
        }
    }
    if (m_selected == target && m_window->property("screenReady").toBool()) {
        return;
    }
    suspend();
    if (m_selected) { disconnect(m_selected, &QScreen::geometryChanged, this, &OverlayController::applyPosition); }
    m_selected = target;
    connect(target, &QScreen::geometryChanged, this, &OverlayController::applyPosition);
    m_window->setScreen(target);
    m_layer->setScreen(target);
    applyPosition();
    m_window->setProperty("screenReady", true);
    qInfo().noquote() << "mic-overlay: screen:" << target->name();
}
