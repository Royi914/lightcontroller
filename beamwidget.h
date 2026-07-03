#ifndef BEAMWIDGET_H
#define BEAMWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPen>
#include <QBrush>

class BeamWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BeamWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumWidth(180);
        setFixedHeight(90);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setBeamAngle(float pan, float tilt) { m_pan = pan; m_tilt = tilt; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        int w = width(), h = height();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x18, 0x18, 0x22));
        p.drawRoundedRect(0, 0, w, h, 4, 4);

        p.setPen(QColor(0x99, 0x99, 0xaa));
        p.setFont(QFont("sans-serif", 8));
        p.drawText(6, 12, "光束朝向");

        // === 等轴立方体 ===
        int cx = 55, cy = h - 26;
        float sx = 26, sy = 18, sz = 20;  // 宽一点
        float ca = 0.259f, sa = 0.966f; // ≈ 75°, 左前视角, 左面更多，避开 45° 重合
        auto proj = [=](float x, float y, float z) -> QPointF {
            return QPointF(cx + (x*ca - z*sa)*0.9f,
                           cy - y*0.65f + (x + z)*0.10f);
        };

        QPointF v[8] = {
            proj(-sx,-sy,-sz), proj( sx,-sy,-sz), proj( sx, sy,-sz), proj(-sx, sy,-sz),
            proj(-sx,-sy, sz), proj( sx,-sy, sz), proj( sx, sy, sz), proj(-sx, sy, sz),
        };

        // 面（半透明填充）
        auto drawFace = [&](int a,int b,int c,int d, const QColor &fill) {
            QPolygonF poly;
            poly << v[a] << v[b] << v[c] << v[d];
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawPolygon(poly);
        };
        drawFace(0,4,5,1, QColor(100,150,255,30));
        drawFace(0,1,2,3, QColor(150,180,255,45));
        drawFace(1,5,6,2, QColor(120,160,255,35));
        drawFace(3,2,6,7, QColor(180,200,255,50));

        // 12 条棱
        struct Edge { int a,b; };
        // 实线（前、右、可见面）
        static const Edge solid[] = {
            {0,1},{1,2},{2,3},{3,0}, // 底面
            {0,4},{1,5},{2,6},       // 前/右竖棱
            {5,6},{6,7},             // 右顶 + 后顶
        };
        // 虚线（后面、左后面被遮挡）
        static const Edge dash[] = {
            {4,5},{7,4},             // 后底 + 后左上
            {3,7},                   // 左后竖棱
        };

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0xcc, 0xdd, 0xff), 1.5f));
        for (auto &e : solid) p.drawLine(v[e.a], v[e.b]);

        QPen dashPen(QColor(0x88, 0x99, 0xcc), 1.2f, Qt::DashLine);
        p.setPen(dashPen);
        for (auto &e : dash) p.drawLine(v[e.a], v[e.b]);

        // 角度标签
        p.setPen(QColor(0x88, 0x88, 0x99));
        p.setFont(QFont("sans-serif", 8));
        p.drawText(cx + (int)sx + 32, cy - 6, QString("Pan: %1°").arg(m_pan, 0, 'f', 0));
        p.drawText(cx + (int)sx + 32, cy + 12, QString("Tilt: %1°").arg(m_tilt, 0, 'f', 0));
    }

private:
    float m_pan = 0, m_tilt = 0;
};

#endif // BEAMWIDGET_H
