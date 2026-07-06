#ifndef TIMELINE_H
#define TIMELINE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QList>

class TimelineBlock : public QWidget
{
public:
    explicit TimelineBlock(const QString &name, QWidget *parent = nullptr)
        : QWidget(parent), m_name(name)
    {
        setFixedHeight(30); setFixedWidth(300); setCursor(Qt::OpenHandCursor);
        updateStyle();
        auto *hl = new QHBoxLayout(this); hl->setContentsMargins(6,0,6,0);
        m_label = new QLabel(name + "  5.0s");
        m_label->setStyleSheet("color:#222;font-size:10px;border:none;background:transparent");
        hl->addWidget(m_label); hl->addStretch();
    }
    QString fixtureName() const { return m_name; }
    double posSec() const { return m_pos; }
    double durSec()  const { return m_dur; }
    void setPosSec(double s) { m_pos = qMax(s, 0.0); move(int(m_pos * 60), 0); }
    void setDurSec(double d) { m_dur = qMax(d, 0.5); setFixedWidth(int(m_dur * 60)); updateStyle(); }

    void updateStyle() {
        setStyleSheet("background:#f0c040;border-radius:4px;border:1px solid #c09020");
        if (m_label) m_label->setText(QString("%1  %2s").arg(m_name).arg(m_dur, 0, 'f', 1));
    }
    QString m_name; QLabel *m_label;
    double m_pos = 0, m_dur = 5;
};

class TimelineTrack : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineTrack(const QString &name, QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(40); setMouseTracking(true);
        setStyleSheet("background:#fff;border-bottom:1px solid #ddd");
        auto *hl = new QHBoxLayout(this); hl->setContentsMargins(4,2,4,2);
        hl->addWidget(new QLabel(name));
        m_blockArea = new QWidget; m_blockArea->setStyleSheet("background:transparent");
        hl->addWidget(m_blockArea, 1);
    }
    void addBlock(const QString &name) {
        auto *b = new TimelineBlock(name, m_blockArea);
        if (!m_blocks.isEmpty()) {
            auto *last = m_blocks.last();
            b->setPosSec(last->posSec() + last->durSec());
        }
        b->show(); m_blocks << b;
    }

protected:
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton) return;
        QPoint lp = m_blockArea->mapFrom(this, e->position().toPoint());
        m_dragBlock = blockAt(lp);
        if (!m_dragBlock) return;
        m_dragging = true; m_dragStart = e->globalPosition().toPoint();
        m_resizing = (lp.x() > m_dragBlock->x() + m_dragBlock->width() - 10);
        m_origPos = m_dragBlock->posSec(); m_origDur = m_dragBlock->durSec();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (!m_dragging || !m_dragBlock) return;
        int dx = e->globalPosition().toPoint().x() - m_dragStart.x();
        if (m_resizing) {
            double nd = qMax(m_origDur + dx / 60.0, 0.5);
            if (!wouldOverlap(m_dragBlock, m_dragBlock->posSec(), nd))
                m_dragBlock->setDurSec(nd);
        } else {
            double np = qMax(m_origPos + dx / 60.0, 0.0);
            if (!wouldOverlap(m_dragBlock, np, m_dragBlock->durSec()))
                m_dragBlock->setPosSec(np);
        }
    }
    void mouseReleaseEvent(QMouseEvent *) override { m_dragging = false; m_dragBlock = nullptr; }

private:
    TimelineBlock *blockAt(const QPoint &lp) {
        for (auto *b : m_blocks)
            if (lp.x() >= b->x() && lp.x() <= b->x() + b->width()) return b;
        return nullptr;
    }
    bool wouldOverlap(TimelineBlock *self, double pos, double dur) {
        for (auto *other : m_blocks) {
            if (other == self) continue;
            if (pos < other->posSec() + other->durSec() &&
                other->posSec() < pos + dur) return true;
        }
        return false;
    }

    QWidget *m_blockArea;
    QList<TimelineBlock *> m_blocks;
    TimelineBlock *m_dragBlock = nullptr;
    QPoint m_dragStart; double m_origPos = 0, m_origDur = 5;
    bool m_dragging = false, m_resizing = false;
};

class Timeline : public QWidget
{
public:
    explicit Timeline(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(0);
        auto *topBar = new QHBoxLayout;
        topBar->addWidget(new QLabel("时间线"));
        auto *addBtn = new QPushButton("+ 轨道"); connect(addBtn, &QPushButton::clicked, this, &Timeline::addTrack); topBar->addWidget(addBtn);
        auto *delBtn = new QPushButton("- 轨道"); connect(delBtn, &QPushButton::clicked, this, &Timeline::removeTrack); topBar->addWidget(delBtn);
        topBar->addStretch(); auto *topFrame = new QWidget; topFrame->setLayout(topBar); root->addWidget(topFrame);
        m_trackContainer = new QWidget; m_trackLayout = new QVBoxLayout(m_trackContainer);
        for (int i = 1; i <= 3; i++) addTrack();
        auto *scroll = new QScrollArea; scroll->setWidgetResizable(true);
        scroll->setWidget(m_trackContainer); root->addWidget(scroll, 1);
    }
    void addBlockToFirstTrack(const QString &name) {
        if (m_trackLayout->count() == 0) return;
        auto *t = qobject_cast<TimelineTrack *>(m_trackLayout->itemAt(0)->widget());
        if (t) t->addBlock(name);
    }
private:
    void addTrack() { m_trackLayout->addWidget(new TimelineTrack(QString("轨道 %1").arg(++m_trackCount))); }
    void removeTrack() { if (m_trackCount<=1) return; auto *it=m_trackLayout->takeAt(m_trackLayout->count()-1); delete it->widget(); delete it; m_trackCount--; }
    QWidget *m_trackContainer; QVBoxLayout *m_trackLayout; int m_trackCount = 0;
};

#endif
