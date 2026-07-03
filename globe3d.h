#ifndef GLOBE3D_H
#define GLOBE3D_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVector3D>
#include <QMatrix4x4>
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
    QVector3D target = QVector3D(0, 0, 0);
    float     distance = 25.0f;
    float     azimuth  = 45.0f;
    float     altitude = 30.0f;

    QMatrix4x4 viewMatrix() const;
    void rotate(float dAz, float dAlt);
    void zoom(float delta);
    void pan(float dx, float dy);
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
    void clear();

signals:
    void fixtureSelected(Fixture *f);
    void fixtureDeselected();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void drawGrid();
    void drawSphere(const QVector3D &pos, float r, const QColor &color, bool selected);
    Fixture *pickFixture(const QPoint &screenPos);

    OrbitCamera m_camera;
    QList<GlobeFixture> m_fixtures;
    Fixture *m_selected = nullptr;
    QPoint m_lastMouse;
    bool   m_leftPressed = false, m_rightPressed = false, m_midPressed = false;
};

#endif
