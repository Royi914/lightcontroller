#include "globe3d.h"
#include "Fixture.h"
#include <QtMath>

QMatrix4x4 OrbitCamera::viewMatrix() const
{
    QMatrix4x4 m;
    m.translate(0, 0, -distance);
    m.rotate(-altitude, 1, 0, 0);
    m.rotate(-azimuth, 0, 1, 0);
    m.translate(-target);
    return m;
}
void OrbitCamera::rotate(float dAz, float dAlt) { azimuth += dAz; altitude += dAlt; if (altitude > 89) altitude = 89; if (altitude < 5) altitude = 5; }
void OrbitCamera::zoom(float d) { distance -= d; if (distance < 3) distance = 3; if (distance > 100) distance = 100; }
void OrbitCamera::pan(float dx, float dy) { target += QVector3D(dx, dy, 0); }

Globe3D::Globe3D(QWidget *parent) : QOpenGLWidget(parent) { setMinimumSize(400, 300); setMouseTracking(true); }
Globe3D::~Globe3D() = default;

void Globe3D::addFixture(Fixture *f, const QPointF &pos2D)
{
    GlobeFixture gf; gf.fixture = f;
    gf.position = QVector3D(pos2D.x() / 40.0f, 0, pos2D.y() / 40.0f);
    m_fixtures << gf; update();
}
void Globe3D::removeFixture(Fixture *f)
{
    for (int i = 0; i < m_fixtures.size(); i++)
        if (m_fixtures[i].fixture == f) { m_fixtures.removeAt(i); break; }
    if (m_selected == f) m_selected = nullptr; update();
}
void Globe3D::updateFixture(Fixture *)
{
    // 2D/3D 圆圈始终亮白，不随通道值变化
    for (auto &gf : m_fixtures)
        gf.color = Qt::white;
    update();
}
void Globe3D::clear() { m_fixtures.clear(); m_selected = nullptr; update(); }

void Globe3D::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.96f, 0.96f, 0.96f, 1.0f);
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat lp[] = { 10, 20, 10, 1 }, la[] = { 0.2f, 0.2f, 0.25f, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, lp); glLightfv(GL_LIGHT0, GL_AMBIENT, la);
}

void Globe3D::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h); glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float fov = 45.0f, asp = float(w)/h;
    float f = 1.0f / tan(fov * M_PI / 360.0f);
    float proj[16] = { f/asp,0,0,0, 0,f,0,0, 0,0,-1.002f,-1, 0,0,-1.0f,0 };
    glMultMatrixf(proj);
    glMatrixMode(GL_MODELVIEW);
}

void Globe3D::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    QMatrix4x4 v = m_camera.viewMatrix(); float mv[16];
    for (int i = 0; i < 16; i++) mv[i] = v.constData()[i];
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glMultMatrixf(mv);
    drawGrid();
    for (auto &gf : m_fixtures) drawSphere(gf.position, gf.radius, gf.color, gf.selected);
}

void Globe3D::drawGrid()
{
    glDisable(GL_LIGHTING); glColor3f(0.18f, 0.18f, 0.22f); glLineWidth(1);
    glBegin(GL_LINES);
    for (int i = -10; i <= 10; i++) {
        glVertex3f(i, 0, -10); glVertex3f(i, 0, 10);
        glVertex3f(-10, 0, i); glVertex3f(10, 0, i);
    }
    glEnd(); glEnable(GL_LIGHTING);
}

void Globe3D::drawSphere(const QVector3D &pos, float r, const QColor &color, bool selected)
{
    glPushMatrix(); glTranslatef(pos.x(), pos.y() + r, pos.z());
    float d = color.redF(); glColor3f(d*0.8f, d*0.8f, d*0.8f);
    int st = 16, sl = 16;
    for (int i = 0; i < st; i++) {
        float lat0 = M_PI * (-0.5f + float(i)/st), lat1 = M_PI * (-0.5f + float(i+1)/st);
        float y0 = sin(lat0), y1 = sin(lat1), r0 = cos(lat0), r1 = cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= sl; j++) {
            float lng = 2*M_PI*float(j)/sl, x = cos(lng), z = sin(lng);
            glNormal3f(x*r0, y0, z*r0); glVertex3f(x*r0*r, y0*r, z*r0*r);
            glNormal3f(x*r1, y1, z*r1); glVertex3f(x*r1*r, y1*r, z*r1*r);
        }
        glEnd();
    }
    if (selected) {
        glDisable(GL_LIGHTING); glColor3f(1,1,0); glLineWidth(3);
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j < 64; j++) glVertex3f(cos(2*M_PI*j/63)*r*1.25f, 0, sin(2*M_PI*j/63)*r*1.25f);
        glEnd(); glEnable(GL_LIGHTING);
    }
    glPopMatrix();
}

void Globe3D::mousePressEvent(QMouseEvent *e)
{
    m_lastMouse = e->pos();
    if (e->button() == Qt::LeftButton)   m_leftPressed  = true;
    if (e->button() == Qt::RightButton)  m_rightPressed = true;
    if (e->button() == Qt::MiddleButton) m_midPressed   = true;
    if (e->button() == Qt::LeftButton) {
        Fixture *f = pickFixture(e->pos());
        if (f) {
            m_selected = f;
            for (auto &gf : m_fixtures) gf.selected = (gf.fixture == f);
            emit fixtureSelected(f);
        } else {
            m_selected = nullptr;
            for (auto &gf : m_fixtures) gf.selected = false;
            emit fixtureDeselected();
        }
        update();
    }
}

void Globe3D::mouseMoveEvent(QMouseEvent *e)
{
    float dx = e->pos().x() - m_lastMouse.x(), dy = e->pos().y() - m_lastMouse.y();
    m_lastMouse = e->pos();
    if (m_leftPressed && m_selected) {
        for (auto &gf : m_fixtures) if (gf.fixture == m_selected) {
            gf.position += QVector3D(dx * 0.05f, 0, -dy * 0.05f);
            update(); break;
        }
    }
    if (m_rightPressed) { m_camera.rotate(dx * 0.3f, dy * 0.3f); update(); }
    if (m_midPressed)   { m_camera.pan(-dx * 0.05f, dy * 0.05f); update(); }
}

void Globe3D::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)   m_leftPressed  = false;
    if (e->button() == Qt::RightButton)  m_rightPressed = false;
    if (e->button() == Qt::MiddleButton) m_midPressed   = false;
}

void Globe3D::wheelEvent(QWheelEvent *e) { m_camera.zoom(e->angleDelta().y() * 0.03f); update(); }

Fixture *Globe3D::pickFixture(const QPoint &screenPos)
{
    QMatrix4x4 view = m_camera.viewMatrix();
    float best = 40.0f; Fixture *found = nullptr;
    for (auto &gf : m_fixtures) {
        QVector4D c = view * QVector4D(gf.position.x(), gf.position.y()+gf.radius, gf.position.z(), 1.0f);
        if (c.z() >= 0) continue;
        float sx = (c.x()/-c.z() + 1.0f) * 0.5f * width();
        float sy = (1.0f - c.y()/-c.z()) * 0.5f * height();
        float d = sqrt(pow(screenPos.x()-sx, 2) + pow(screenPos.y()-sy, 2));
        if (d < best) { best = d; found = gf.fixture; }
    }
    return found;
}
