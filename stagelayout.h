#ifndef STAGELAYOUT_H
#define STAGELAYOUT_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSet>
#include <QVector>
#include <QRectF>
#include <QString>
#include <QFont>
#include <QPushButton>

struct StagePos {
    QString name;
    QRectF rect;
    QString group;
    int index;
};

class StageLayout : public QWidget
{
    Q_OBJECT
public:
    explicit StageLayout(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(1080, 500);
        initPositions();

        m_enterBtn = new QPushButton("进入编程 →", this);
        m_enterBtn->setFixedSize(140, 36);
        m_enterBtn->setCursor(Qt::PointingHandCursor);
        m_enterBtn->setStyleSheet(
            "QPushButton { background:#35a; color:#fff; border:1px solid #249;"
            "border-radius:4px; font-size:14px; font-weight:bold; }"
            "QPushButton:hover { background:#46b; }");
        connect(m_enterBtn, &QPushButton::clicked, this, &StageLayout::enterProgramRequested);
    }

    QSize sizeHint() const override { return QSize(1120, 500); }

    QStringList selectedNames() const
    {
        QStringList names;
        for (int i : m_selected) names << m_positions[i].name;
        return names;
    }

    int selectedCount() const { return m_selected.size(); }
    int totalCount() const { return m_positions.size(); }

    void selectAll()
    {
        m_selected.clear();
        for (int i = 0; i < m_positions.size(); i++) m_selected.insert(i);
        update();
        emit selectionChanged();
    }

    void clearSelection()
    {
        m_selected.clear();
        update();
        emit selectionChanged();
    }

    // ---- 占用状态 ----
    void assignPosition(const QString &name)
    {
        m_occupied.insert(name);
        update();
    }

    void unassignPosition(const QString &name)
    {
        m_occupied.remove(name);
        update();
    }

    bool isOccupied(const QString &name) const { return m_occupied.contains(name); }

    int maxNumber(const QString &group) const
    {
        int n = 0;
        for (const auto &p : m_positions)
            if (p.group == group && p.index + 1 > n) n = p.index + 1;
        return n;
    }

    QStringList availableNames(const QString &group) const
    {
        QStringList names;
        for (const auto &p : m_positions)
            if (p.group == group && !m_occupied.contains(p.name))
                names << p.name;
        return names;
    }

    QStringList allNames(const QString &group) const
    {
        QStringList names;
        for (const auto &p : m_positions)
            if (p.group == group) names << p.name;
        return names;
    }

signals:
    void selectionChanged();
    void enterProgramRequested();

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        // Reposition rects (they use width()/height() in init)
        initPositions();
        // Reposition button to bottom-right
        m_enterBtn->move(width() - m_enterBtn->width() - 20,
                         height() - m_enterBtn->height() - 16);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(0xee, 0xee, 0xee));

        for (int i = 0; i < m_positions.size(); i++) {
            const auto &pos = m_positions[i];
            bool sel = m_selected.contains(i);
            bool occ = m_occupied.contains(pos.name);
            QRectF r = pos.rect;

            // Fill
            p.setPen(Qt::NoPen);
            if (sel)
                p.setBrush(QColor(0x55, 0x88, 0xee));
            else if (occ)
                p.setBrush(QColor(0xe8, 0xf5, 0xe8));
            else
                p.setBrush(QColor(0xff, 0xff, 0xff));
            p.drawRoundedRect(r, 3, 3);

            // Border
            QColor borderCol;
            int borderW;
            if (sel)      { borderCol = QColor(0x33, 0x55, 0xbb); borderW = 2; }
            else if (occ) { borderCol = QColor(0x4a, 0x8a, 0x4a); borderW = 2; }
            else          { borderCol = QColor(0xbb, 0xbb, 0xbb); borderW = 1; }
            p.setPen(QPen(borderCol, borderW));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r, 3, 3);

            // Occupied dot
            if (occ && !sel) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0x4a, 0xaa, 0x4a));
                p.drawEllipse(QPointF(r.right() - 8, r.top() + 8), 4, 4);
            }

            p.setPen(sel ? QColor(0xff, 0xff, 0xff) : QColor(0x33, 0x33, 0x33));
            QFont f = font();
            f.setPixelSize(15);
            f.setBold(sel);
            p.setFont(f);
            p.drawText(r, Qt::AlignCenter, pos.name);
        }

        // Group labels
        QFont sf = font();
        sf.setPixelSize(12);
        p.setFont(sf);
        p.setPen(QColor(0xaa, 0xaa, 0xaa));

        QRectF topRects = groupRect("逆光");
        if (topRects.isValid())
            p.drawText(QRectF(topRects.left(), topRects.top() - 22, topRects.width(), 18),
                       Qt::AlignHCenter | Qt::AlignBottom, "逆 光");

        QRectF botRects = groupRect("面光");
        if (botRects.isValid())
            p.drawText(QRectF(botRects.left(), botRects.bottom() + 4, botRects.width(), 18),
                       Qt::AlignHCenter | Qt::AlignTop, "面 光");

        QRectF leftRects = subgroupRect("侧光", 0, 3);
        if (leftRects.isValid())
            p.drawText(QRectF(leftRects.left() - 24, leftRects.top(), 22, leftRects.height()),
                       Qt::AlignCenter, "侧\n光");

        QRectF rightRects = subgroupRect("侧光", 3, 3);
        if (rightRects.isValid())
            p.drawText(QRectF(rightRects.right() + 2, rightRects.top(), 22, rightRects.height()),
                       Qt::AlignCenter, "侧\n光");

        QRectF topRects2 = subgroupRect("顶光", 0, 5);
        if (topRects2.isValid())
            p.drawText(QRectF(topRects2.left(), topRects2.top() - 22, topRects2.width(), 18),
                       Qt::AlignHCenter | Qt::AlignBottom, "顶 光");
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) return;
        QPointF pt = event->position();
        for (int i = 0; i < m_positions.size(); i++) {
            if (m_positions[i].rect.contains(pt)) {
                if (m_selected.contains(i))
                    m_selected.remove(i);
                else
                    m_selected.insert(i);
                update();
                emit selectionChanged();
                return;
            }
        }
    }

private:
    void initPositions()
    {
        m_positions.clear();
        qreal rx = 120, ry = 48;
        qreal gap = 10;
        qreal w = qreal(width()), h = qreal(height());

        // 紧凑居中：四排矩形垂直居中
        qreal rowGap = 24;
        qreal totalH = 4 * ry + 3 * rowGap;  // 逆光 + 2顶光 + 面光
        qreal startY = (h - totalH) / 2.0;

        qreal backY   = startY;                      // 逆光
        qreal top1Y   = backY + ry + rowGap;          // 顶光 1
        qreal top2Y   = top1Y + ry + rowGap;          // 顶光 2
        qreal frontY  = top2Y + ry + rowGap;          // 面光

        // 逆光 1-6
        qreal topStartX = (w - (6 * rx + 5 * gap)) / 2.0;
        for (int i = 0; i < 6; i++)
            m_positions << StagePos{QString("逆光%1").arg(i + 1),
                                     QRectF(topStartX + i * (rx + gap), backY, rx, ry), "逆光", i};

        // 面光 1-6
        qreal botStartX = (w - (6 * rx + 5 * gap)) / 2.0;
        for (int i = 0; i < 6; i++)
            m_positions << StagePos{QString("面光%1").arg(i + 1),
                                     QRectF(botStartX + i * (rx + gap), frontY, rx, ry), "面光", i};

        // 侧光贴紧中间区域两侧
        qreal leftX = topStartX - rx - 16;
        qreal rightX = topStartX + 6 * rx + 5 * gap + 16;
        qreal sideTop = backY + ry + 10, sideBot = frontY - 10;
        qreal sideGap = 40;
        qreal sideTotalH = 3 * ry + 2 * sideGap;
        qreal sideStartY = sideTop + (sideBot - sideTop - sideTotalH) / 2.0;
        for (int i = 0; i < 3; i++) {
            m_positions << StagePos{QString("侧光%1").arg(i + 1),
                                     QRectF(leftX, sideStartY + i * (ry + sideGap), rx, ry), "侧光", i};
            m_positions << StagePos{QString("侧光%1").arg(i + 4),
                                     QRectF(rightX, sideStartY + i * (ry + sideGap), rx, ry), "侧光", i + 3};
        }

        // 顶光 1-5 + 6-10
        qreal topStartX2 = (w - (5 * rx + 4 * gap)) / 2.0;
        for (int i = 0; i < 5; i++) {
            m_positions << StagePos{QString("顶光%1").arg(i + 1),
                                     QRectF(topStartX2 + i * (rx + gap), top1Y, rx, ry), "顶光", i};
            m_positions << StagePos{QString("顶光%1").arg(i + 6),
                                     QRectF(topStartX2 + i * (rx + gap), top2Y, rx, ry), "顶光", i + 5};
        }
    }

    QRectF groupRect(const QString &group) const
    {
        QRectF result; bool first = true;
        for (const auto &p : m_positions) {
            if (p.group != group) continue;
            if (first) { result = p.rect; first = false; }
            else result = result.united(p.rect);
        }
        return result;
    }

    QRectF subgroupRect(const QString &group, int startIdx, int count) const
    {
        QRectF result; bool first = true;
        for (const auto &p : m_positions) {
            if (p.group != group || p.index < startIdx || p.index >= startIdx + count) continue;
            if (first) { result = p.rect; first = false; }
            else result = result.united(p.rect);
        }
        return result;
    }

    QVector<StagePos> m_positions;
    QSet<int> m_selected;
    QSet<QString> m_occupied;
    QPushButton *m_enterBtn = nullptr;
};

#endif // STAGELAYOUT_H
