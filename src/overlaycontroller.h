#pragma once

#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QSize>
#include <QVariantMap>
#include <QScreen>
#include <QTimer>
#include <QWindow>

namespace LayerShellQt { class Window; }

class OverlayController : public QObject
{
    Q_OBJECT
public:
    OverlayController(QWindow *window, LayerShellQt::Window *layer,
                      const QString &screenSelection);
    void setPosition(int x, int y);
    QPoint position() const;
    void moveBy(double dx, double dy);
    void setScreenSelection(const QString &selection);
public slots:
    void refreshGeometry() { applyPosition(); }
    void applyConfiguration(const QVariantMap &values);
    void previewSetting(const QString &key, const QVariant &value);
    void setEditing(bool editing);
    void moveOverlay(double dx, double dy);
    void resetPosition();
    void finishReposition() { emit positionSaveRequested(); }

signals:
    void positionSaveRequested();

private:
    friend class OverlayControllerTest;
    void reconcile(const QList<QScreen *> &screens, QScreen *primary);
    void suspend();
    void applyPosition();
    QSize indicatorSize() const;
    int m_x = -1;
    int m_y = -1;
    QPointF m_fraction;
    QPointer<QWindow> m_window;
    LayerShellQt::Window *m_layer;
    QPointer<QScreen> m_selected;
    QString m_selection;
    QTimer m_refresh;
    bool m_warnedMissing = false;
};
