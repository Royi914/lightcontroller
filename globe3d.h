#ifndef GLOBE3D_H
#define GLOBE3D_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVector3D>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QPushButton>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QList>

class Fixture;

struct GlobeFixture {
    Fixture *fixture;
    QVector3D position;
    float    radius = 0.5f;
    QColor   color = Qt::white;
    bool     selected = false;
};

struct OrbitCamera {
    QVector3D   target   = QVector3D(0, 0, 0);
    float       distance = 35.0f;
    QQuaternion rotation;  // accumulated trackball rotation (360° no limits)

    QMatrix4x4 viewMatrix() const;
    void rotate(float dx, float dy);
    void zoom(float delta);
    void pan(float dx, float dy);
    void reset() { target = QVector3D(0,0,0); distance = 35.0f; rotation = QQuaternion(); }
};

class Globe3D : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit Globe3D(QWidget *parent = nullptr);
    ~Globe3D();

    void addFixture(Fixture *f, const QPointF &pos2D = QPointF());
    void removeFixture(Fixture *f);
    void updateFixture(Fixture *f);
    void updateFixturePosition(Fixture *f, const QPointF &pos2D);
    void clear();

signals:
    void fixtureSelected(Fixture *f);
    void fixtureDeselected();
    void fixtureMoved3D(Fixture *f, QPointF pos2D);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void resizeEvent(QResizeEvent *e) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private slots:
    void onResetView();

private:
    void drawGrid();
    void drawSphere(const QVector3D &pos, float r, const QColor &color, bool selected);
    Fixture *pickFixture(const QPoint &screenPos);
    QPointF screenToGround(const QPoint &screenPos);  // screen → Y=0 plane intersection

    OrbitCamera m_camera;
    QList<GlobeFixture> m_fixtures;
    Fixture *m_selected = nullptr;
    QPoint m_lastMouse;
    bool   m_leftPressed = false, m_rightPressed = false, m_midPressed = false;
    QPushButton *m_resetBtn = nullptr;
};

#endif
