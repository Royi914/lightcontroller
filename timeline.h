#ifndef TIMELINE_H
#define TIMELINE_H

#include <QWidget>
#include <QGraphicsWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsLinearLayout>
#include <QGraphicsSceneMouseEvent>
#include <QCoreApplication>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>
#include <QtMath>
#include <limits>
#include <QList>
#include <QTimer>
#include <QElapsedTimer>
#include <QGraphicsLineItem>

// ============================================================
//  Constants
// ============================================================
static constexpr qreal PX_PER_SEC       = 60.0;
static constexpr qreal BLOCK_MIN_W      = 30.0;   // 0.5 s
static constexpr qreal BLOCK_H          = 28.0;
static constexpr qreal TRACK_H          = 40.0;
static constexpr qreal RULER_H          = 30.0;
static constexpr qreal RESIZE_MARGIN    = 8.0;
static constexpr qreal TRACK_LABEL_W    = 60.0;   // 轨道标签 + 刻度尺左边距
static constexpr qreal TRACK_SWITCH_DY  = 15.0;   // 上下拖动阈值

// ============================================================
//  1. GraphicsViewScalable  — 可水平缩放的 QGraphicsView
//     参考 WidgetComposition graphicsviewscalable.h
// ============================================================
class GraphicsViewScalable : public QGraphicsView
{
    Q_OBJECT
public:
    explicit GraphicsViewScalable(QWidget *parent = nullptr)
        : QGraphicsView(parent)
    {
        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        setFrameShape(QFrame::NoFrame);
    }

    void zoomIn()  { scaleView(1.0 / 1.2); }
    void zoomOut() { scaleView(1.2); }

    bool scaleView(qreal factor)
    {
        QTransform t = transform().scale(factor, 1.0);
        qreal m11 = t.mapRect(QRectF(0, 0, 1, 1)).width();
        if (m11 < 0.007 || m11 > 50)
            return false;

        QRectF vr = viewport()->rect();
        if (sceneRect().width() * t.m11() - vr.width() < 0) {
            // Shrink to fit
            qreal s = vr.width() / sceneRect().width();
            QTransform m(s, transform().m12(), transform().m21(), transform().m22(),
                         transform().dx(), transform().dy());
            setTransform(m, false);
        } else {
            setTransform(t);
        }
        return true;
    }

    qreal currentScale() const { return transform().m11(); }
};

// ============================================================
//  Forward declarations
// ============================================================
class BlockWidget;

// ============================================================
//  2. RulerWidget  — 时间刻度尺
//     参考 WidgetComposition TimelinePainter::paint()
// ============================================================
class RulerWidget : public QGraphicsWidget
{
public:
    explicit RulerWidget(QGraphicsItem *parent = nullptr)
        : QGraphicsWidget(parent)
    {
        setFlags(QGraphicsItem::ItemUsesExtendedStyleOption);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override
    {
        const qreal lod = option->levelOfDetailFromTransform(painter->worldTransform());
        const QRectF exposed = option->exposedRect;
        const qreal h = boundingRect().height();

        // Background: dark area for track labels, then timeline bg
        painter->fillRect(QRectF(0, 0, TRACK_LABEL_W, h), QColor(0x42, 0x42, 0x42));
        painter->fillRect(QRectF(TRACK_LABEL_W, 0, exposed.width(), h), QColor(33, 33, 33));

        // Divider line
        painter->setPen(QPen(QColor(0x50, 0x50, 0x50), 0));
        painter->drawLine(QPointF(TRACK_LABEL_W, 0), QPointF(TRACK_LABEL_W, h));

        QPen pen;
        pen.setCosmetic(true);
        pen.setColor(QColor(120, 120, 125));
        pen.setWidth(0);
        painter->setPen(pen);

        QFont font("Arial");
        font.setPixelSize(11);
        font.setBold(true);
        painter->setFont(font);

        const qreal tickShort = h * 0.55;
        const qreal tickFull  = h;

        // Tick spacing uses fixed scene coords (60px/s), lod only picks the zoom tier
        if (PX_PER_SEC * lod >= 4) {
            drawTicks(painter, exposed, PX_PER_SEC, 1, "s", 5, tickShort, tickFull, h);
        } else if (PX_PER_SEC * 10 * lod >= 2) {
            drawTicks(painter, exposed, PX_PER_SEC * 10, 10, "s", 6, tickShort, tickFull, h);
        } else if (PX_PER_SEC * 60 * lod >= 2) {
            drawTicks(painter, exposed, PX_PER_SEC * 60, 60, "s", 0, tickShort, tickFull, h);
        } else {
            drawTicks(painter, exposed, PX_PER_SEC * 300, 300, "s", 0, tickShort, tickFull, h);
        }
    }

    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &) const override
    {
        if (which == Qt::PreferredSize || which == Qt::MinimumSize)
            return QSizeF(0, RULER_H);
        return QSizeF(std::numeric_limits<qint32>::max(), RULER_H);
    }

private:
    void drawTicks(QPainter *p, const QRectF &exposed, qreal pxStep,
                   int stepVal, const QString &suffix, int majorEvery,
                   qreal y1, qreal y2, qreal rulerH) const
    {
        const qreal L = exposed.left(), R = exposed.right();
        const qreal textW = 60, textH = 12, textY = 2;
        const qreal origin = TRACK_LABEL_W;  // 0s 偏移到标签右侧

        qint32 start = qMax(qint32((L - origin) / pxStep) - 1, 0);
        for (qint32 i = start; (origin + i * pxStep) <= R + pxStep; i++) {
            qreal x = origin + i * pxStep;
            bool isMajor = (majorEvery > 0) && (i % majorEvery == 0);

            p->drawLine(QPointF(x, isMajor ? rulerH * 0.25 : y1), QPointF(x, y2));

            if (isMajor && pxStep > 40) {
                QTransform t = p->transform();
                p->setTransform(QTransform(t).scale(1.0 / t.m11(), 1.0));
                QString label = QString::number(i * stepVal) + suffix;
                p->setPen(QColor(200, 200, 200));
                p->drawText(QRectF(x * t.m11() - textW / 2, textY, textW, textH),
                            Qt::AlignCenter, label);
                p->setPen(QColor(120, 120, 125));
                p->setTransform(t);
            }
        }
    }
};

// ============================================================
//  3. TrackWidget  — 单条轨道（声明；实现在 BlockWidget 之后）
// ============================================================
class TrackWidget : public QGraphicsWidget
{
public:
    explicit TrackWidget(const QString &name, QGraphicsItem *parent = nullptr);
    void addBlock(const QString &blockName);
    void addExistingBlock(BlockWidget *b);
    void removeBlock(BlockWidget *b);
    void deleteBlock(BlockWidget *b);
    QList<BlockWidget *> blocks() const { return m_blocks; }
    void validateBlock(BlockWidget *self);

    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &) const override;

private:
    static bool overlaps(BlockWidget *a, BlockWidget *b);

    QString             m_name;
    QList<BlockWidget *> m_blocks;
};

// ============================================================
//  4. BlockWidget  — 可拖拽 + 可 resize + 可切换轨道的时间块
// ============================================================
class BlockWidget : public QGraphicsWidget
{
public:
    BlockWidget(const QString &name, TrackWidget *track, QGraphicsItem *parent = nullptr)
        : QGraphicsWidget(parent), m_track(track), m_name(name)
    {
        setAcceptHoverEvents(true);
        setCursor(Qt::OpenHandCursor);
        setFlag(QGraphicsItem::ItemIsSelectable);
        applyGeo();
    }

    QString    name()   const { return m_name; }
    double     posSec() const { return m_posSec; }
    double     durSec() const { return m_durSec; }
    TrackWidget *track() const { return m_track; }

    void setTrack(TrackWidget *t) { m_track = t; }
    void setPosSec(double s) { m_posSec = qMax(s, 0.0); applyGeo(); }
    void setDurSec(double d) { m_durSec = qMax(d, 0.5); applyGeo(); }

    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &) const override
    {
        if (which == Qt::PreferredSize)
            return QSizeF(qMax(m_durSec * PX_PER_SEC, BLOCK_MIN_W), BLOCK_H);
        return QGraphicsWidget::sizeHint(which, QSizeF());
    }

    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        QRectF r = boundingRect().adjusted(1, 1, -1, -1);
        p->setRenderHint(QPainter::Antialiasing);
        bool ghost = m_trackSwitching && !m_overTarget;

        // Fill
        p->setPen(QPen(QColor(0xc0, 0x90, 0x20), 0));
        p->setBrush(ghost ? QColor(0xf0, 0xc0, 0x40, 120)
                          : QColor(0xf0, 0xc0, 0x40));
        p->drawRoundedRect(r, 4, 4);

        // Selection highlight
        if (isSelected()) {
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(QColor(0x00, 0xaa, 0xff), 2.0));
            p->drawRoundedRect(r, 4, 4);
        }

        p->setPen(ghost ? QColor(0x22, 0x22, 0x22, 120) : QColor(0x22, 0x22, 0x22));
        QFont f("Arial"); f.setPixelSize(10); p->setFont(f);
        p->drawText(r.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
                    QString("%1  %2s").arg(m_name).arg(m_durSec, 0, 'f', 1));
    }

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *e) override
    {
        setCursor((size().width() - e->pos().x()) < RESIZE_MARGIN
                      ? Qt::SizeHorCursor : Qt::OpenHandCursor);
    }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override
    {
        setCursor(Qt::OpenHandCursor);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton) return;
        m_dragStart      = e->scenePos();
        m_dragStartScene = e->scenePos();
        m_origPos        = m_posSec;
        m_origDur        = m_durSec;
        m_resizing       = (size().width() - e->pos().x()) < RESIZE_MARGIN;
        m_dragging       = true;
        m_hasMoved       = false;
        m_trackSwitching = false;
        m_overTarget     = false;
        m_origTrack      = m_track;
        m_targetTrack    = nullptr;
        e->accept();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override
    {
        if (!m_dragging) return;

        qreal totalDx = e->scenePos().x() - m_dragStartScene.x();
        qreal totalDy = e->scenePos().y() - m_dragStartScene.y();
        if (qAbs(totalDx) > 3.0 || qAbs(totalDy) > 3.0)
            m_hasMoved = true;

        // Track switching
        if (!m_resizing && (m_trackSwitching || qAbs(totalDy) > TRACK_SWITCH_DY)) {
            if (!m_trackSwitching) {
                m_trackSwitching = true;
                // Record grab point in block-local coords before reparenting
                m_grabPoint = mapFromScene(e->scenePos());
                // Reparent to CompositionWidget so we render above ALL tracks
                QGraphicsItem *comp = m_origTrack->parentItem();
                if (comp) setParentItem(comp);
            }
            // Position so that m_grabPoint stays under the cursor
            QPointF localTarget = parentItem()->mapFromScene(e->scenePos()) - m_grabPoint;
            setPos(localTarget);

            TrackWidget *target = findTrackAtSceneY(e->scenePos().y());
            m_targetTrack = (target && target != m_origTrack) ? target : nullptr;
            m_overTarget  = (m_targetTrack != nullptr);
            update();
            return;
        }

        // Normal horizontal drag (within same track)
        qreal dx = (e->scenePos().x() - m_dragStart.x()) / PX_PER_SEC;
        if (m_resizing) {
            setDurSec(qMax(m_origDur + dx, 0.5));
            m_track->validateBlock(this);
        } else {
            setPosSec(qMax(m_origPos + dx, 0.0));
            m_track->validateBlock(this);
        }
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override
    {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);

        if (m_trackSwitching) {
            if (m_targetTrack) {
                // Move to target track
                m_origTrack->removeBlock(this);
                setParentItem(m_targetTrack);  // scene pos preserved
                m_track = m_targetTrack;
                qreal newPosSec = qMax((scenePos().x() - TRACK_LABEL_W) / PX_PER_SEC, 0.0);
                m_posSec = newPosSec;
                m_targetTrack->addExistingBlock(this);
                m_targetTrack->validateBlock(this);
            } else {
                // Return to original track
                setParentItem(m_origTrack);  // scene pos preserved
                setPosSec(m_origPos);
            }
            m_trackSwitching = false;
            m_targetTrack = nullptr;
            m_overTarget  = false;
            applyGeo();
            update();
        } else if (!m_hasMoved) {
            if (scene()) scene()->clearSelection();
            setSelected(true);
        }
    }

private:
    void applyGeo()
    {
        setPos(TRACK_LABEL_W + m_posSec * PX_PER_SEC,
               (TRACK_H - BLOCK_H) / 2);
        resize(qMax(m_durSec * PX_PER_SEC, BLOCK_MIN_W), BLOCK_H);
        updateGeometry();
    }

    // Find track widget at given scene y coordinate
    TrackWidget *findTrackAtSceneY(qreal sceneY) const
    {
        if (!m_origTrack) return nullptr;
        // All tracks share the same parent (CompositionWidget)
        QGraphicsItem *grandparent = m_origTrack->parentItem();
        if (!grandparent) return nullptr;

        for (auto *child : grandparent->childItems()) {
            auto *tw = dynamic_cast<TrackWidget *>(child);
            if (tw) {
                QPointF local = tw->mapFromScene(QPointF(0, sceneY));
                if (local.y() >= 0 && local.y() <= TRACK_H)
                    return tw;
            }
        }
        return nullptr;
    }

    TrackWidget *m_track;
    TrackWidget *m_origTrack   = nullptr;
    TrackWidget *m_targetTrack = nullptr;
    QString      m_name;
    double       m_posSec      = 0;
    double       m_durSec      = 5;
    QPointF      m_dragStart;
    QPointF      m_dragStartScene;
    QPointF      m_grabPoint;         // mouse pos in block-local coords during switch
    double       m_origPos     = 0;
    double       m_origDur     = 5;
    bool         m_dragging    = false;
    bool         m_resizing    = false;
    bool         m_hasMoved    = false;
    bool         m_trackSwitching = false;
    bool         m_overTarget  = false;
};

// ============================================================
//  TrackWidget out-of-line implementations (needs BlockWidget complete)
// ============================================================

inline TrackWidget::TrackWidget(const QString &name, QGraphicsItem *parent)
    : QGraphicsWidget(parent), m_name(name)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Blocks are direct children, positioned manually (no layout)
}

inline void TrackWidget::addBlock(const QString &blockName)
{
    auto *b = new BlockWidget(blockName, this, this);
    if (!m_blocks.isEmpty()) {
        BlockWidget *last = m_blocks.last();
        b->setPosSec(last->posSec() + last->durSec());
    }
    m_blocks << b;
}

inline void TrackWidget::addExistingBlock(BlockWidget *b)
{
    m_blocks << b;
    b->setTrack(this);
}

inline void TrackWidget::removeBlock(BlockWidget *b)
{
    m_blocks.removeOne(b);
}

inline void TrackWidget::deleteBlock(BlockWidget *b)
{
    m_blocks.removeOne(b);
    if (b->scene()) b->scene()->removeItem(b);
    b->deleteLater();
}

inline void TrackWidget::validateBlock(BlockWidget *self)
{
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto *other : m_blocks) {
            if (other == self) continue;
            if (overlaps(self, other)) {
                self->setPosSec(other->posSec() + other->durSec());
                changed = true;
            }
        }
    }
    if (self->durSec() < 0.5) self->setDurSec(0.5);
}

inline void TrackWidget::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    QRectF r = boundingRect();
    p->fillRect(r, QColor(0x38, 0x38, 0x38));

    // Top divider — clearly separates tracks
    p->fillRect(QRectF(0, 0, r.width(), 3), QColor(0x99, 0x99, 0x99));

    // Track label (left of the 0s origin)
    QRectF labelRect(0, 0, TRACK_LABEL_W - 2, r.height());
    p->fillRect(labelRect, QColor(0x42, 0x42, 0x42));
    p->setPen(QColor(0xbb, 0xbb, 0xbb));
    QFont f("Arial"); f.setPixelSize(10); p->setFont(f);
    p->drawText(labelRect.adjusted(4, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, m_name);

    // Vertical divider line between label and timeline area
    p->setPen(QPen(QColor(0x60, 0x60, 0x60), 0));
    p->drawLine(QPointF(TRACK_LABEL_W, 0), QPointF(TRACK_LABEL_W, r.height()));
}

inline QSizeF TrackWidget::sizeHint(Qt::SizeHint which, const QSizeF &) const
{
    if (which == Qt::PreferredSize || which == Qt::MinimumSize)
        return QSizeF(0, TRACK_H);
    return QSizeF(std::numeric_limits<qint32>::max(), TRACK_H);
}

inline bool TrackWidget::overlaps(BlockWidget *a, BlockWidget *b)
{
    return a->posSec() < b->posSec() + b->durSec() &&
           b->posSec() < a->posSec() + a->durSec();
}

// ============================================================
//  5. CompositionWidget  — 轨道容器 (vertical linear layout)
// ============================================================
class CompositionWidget : public QGraphicsWidget
{
public:
    explicit CompositionWidget(QGraphicsItem *parent = nullptr)
        : QGraphicsWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        m_layout = new QGraphicsLinearLayout(Qt::Vertical);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(2);
        setLayout(m_layout);
    }

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        // Dividers are painted by TrackWidget (top border of each track)
    }

    void addTrack(TrackWidget *t, int index = -1)
    {
        if (index < 0 || index >= m_layout->count())
            m_layout->insertItem(m_layout->count(), t);
        else
            m_layout->insertItem(index, t);
        m_tracks << t;
    }

    void removeTrack(int index)
    {
        if (index < 0 || index >= m_tracks.size()) return;
        TrackWidget *t = m_tracks.takeAt(index);
        m_layout->removeAt(index);
        // Remove all blocks on this track
        for (auto *b : t->blocks()) {
            if (b->scene()) b->scene()->removeItem(b);
            b->deleteLater();
        }
        if (t->scene()) t->scene()->removeItem(t);
        t->deleteLater();
    }

    TrackWidget *track(int i) const
    {
        return (i >= 0 && i < m_tracks.size()) ? m_tracks[i] : nullptr;
    }

    // Find which track contains a given scene Y coordinate
    TrackWidget *trackAtSceneY(qreal sceneY) const
    {
        for (auto *tw : m_tracks) {
            QPointF local = tw->mapFromScene(QPointF(0, sceneY));
            if (local.y() >= 0 && local.y() <= TRACK_H)
                return tw;
        }
        return nullptr;
    }

    int              trackCount() const { return m_tracks.size(); }
    QList<TrackWidget *> tracks() const { return m_tracks; }

private:
    QGraphicsLinearLayout *m_layout = nullptr;
    QList<TrackWidget *>   m_tracks;
};

// ============================================================
//  6. Timeline  — 顶层容器（保持对外 API 兼容）
// ============================================================
class Timeline : public QWidget
{
    Q_OBJECT
public:
    explicit Timeline(QWidget *parent = nullptr);

    /// 保持与旧 API 兼容
    void addBlockToFirstTrack(const QString &name);
    bool hasBlockSelected() const;
    void deleteSelectedBlock();

public slots:
    void addTrack();
    void removeTrack();
    void zoomIn();
    void zoomOut();
    void play();
    void pause();
    void resetPlayhead();
    void seekTo(qreal sceneX);
    void stepPrev();
    void stepNext();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateSceneRects();
    void ensureSceneWidth(qreal neededPx);
    void updatePlayheadLine();

    // Top area: ruler
    QGraphicsScene         *m_rulerScene   = nullptr;
    GraphicsViewScalable   *m_rulerView    = nullptr;
    RulerWidget            *m_ruler        = nullptr;

    // Bottom area: tracks
    QGraphicsScene         *m_trackScene   = nullptr;
    GraphicsViewScalable   *m_trackView    = nullptr;
    CompositionWidget      *m_composition  = nullptr;

    int m_trackCount = 0;

    // Playhead
    QGraphicsLineItem *m_playhead  = nullptr;
    QTimer            *m_playTimer  = nullptr;
    QElapsedTimer      m_playClock;
    qreal              m_playStartX = TRACK_LABEL_W;
    qreal              m_playPos    = TRACK_LABEL_W;
    static constexpr qreal PLAY_SPEED = PX_PER_SEC; // 1s per second
};

// ============================================================
//  Timeline implementation
// ============================================================

inline Timeline::Timeline(QWidget *parent)
    : QWidget(parent)
{
    // ---------- Ruler scene + view ----------
    m_rulerScene = new QGraphicsScene(this);
    m_ruler = new RulerWidget;
    m_rulerScene->addItem(m_ruler);

    m_rulerView = new GraphicsViewScalable;
    m_rulerView->setScene(m_rulerScene);
    m_rulerView->setFixedHeight(int(RULER_H) + 2);
    m_rulerView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rulerView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rulerView->setObjectName("RulerView");
    m_rulerView->setStyleSheet("QFrame#RulerView { background:#212121; border:none; }");

    // ---------- Track scene + view ----------
    m_trackScene = new QGraphicsScene(this);
    m_composition = new CompositionWidget;
    m_trackScene->addItem(m_composition);

    // Playhead: blue vertical line across all tracks
    m_playhead = new QGraphicsLineItem;
    QPen hpPen(QColor(0x33, 0x88, 0xff), 0);
    hpPen.setCosmetic(true);
    hpPen.setWidthF(2.0);
    m_playhead->setPen(hpPen);
    m_playhead->setZValue(100);
    m_playhead->setLine(0, 0, 0, 0);
    m_trackScene->addItem(m_playhead);

    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(33); // ~30 fps
    connect(m_playTimer, &QTimer::timeout, this, [this]() {
        qreal elapsed = qreal(m_playClock.elapsed()) / 1000.0;
        m_playPos = m_playStartX + elapsed * PX_PER_SEC;
        updatePlayheadLine();
        // Smooth scroll: keep playhead in the right 2/3 of view
        if (auto *sb = m_trackView->horizontalScrollBar()) {
            qreal viewW = m_trackView->viewport()->width();
            qreal target = sb->value();
            if (m_playPos > target + viewW * 0.75)
                target = m_playPos - viewW * 0.25;
            // Animate scroll smoothly
            qreal diff = target - sb->value();
            if (qAbs(diff) > 2.0)
                sb->setValue(int(sb->value() + diff * 0.3));
        }
    });

    m_trackView = new GraphicsViewScalable;
    m_trackView->setScene(m_trackScene);
    m_trackView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_trackView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_trackView->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    m_trackView->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    m_trackView->setObjectName("TrackView");
    m_trackView->setStyleSheet(
        "QFrame#TrackView { background:#2a2a2a; border:1px solid #444; }"
        "QScrollBar:horizontal { background:#3a3a3a; height:10px; border-radius:2px; }"
        "QScrollBar::handle:horizontal { background:#888; min-width:40px; border-radius:3px; margin:1px 2px; }"
        "QScrollBar::handle:horizontal:hover { background:#aaa; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background:#2a2a2a; }");

    // Horizontal scroll sync (bidirectional)
    connect(m_trackView->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_rulerView->horizontalScrollBar(), &QScrollBar::setValue);
    connect(m_rulerView->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_trackView->horizontalScrollBar(), &QScrollBar::setValue);

    // ---------- Toolbar ----------
    auto *tb = new QWidget;
    tb->setStyleSheet("background:#2d2d2d;");
    auto *tbl = new QHBoxLayout(tb);
    tbl->setContentsMargins(4, 2, 4, 2);
    tbl->setSpacing(4);

    auto *title = new QLabel("时间线");
    title->setStyleSheet("color:#ccc; font-weight:bold; background:transparent;");
    tbl->addWidget(title);

    const QString btnStyle =
        "QPushButton { background:#3a3a3a; color:#ccc; border:1px solid #555; "
        "border-radius:3px; padding:2px 8px; }"
        "QPushButton:hover { background:#4a4a4a; }";

    auto *addBtn = new QPushButton("+ 轨道");
    addBtn->setStyleSheet(btnStyle);
    connect(addBtn, &QPushButton::clicked, this, &Timeline::addTrack);
    tbl->addWidget(addBtn);

    auto *delBtn = new QPushButton("- 轨道");
    delBtn->setStyleSheet(btnStyle);
    connect(delBtn, &QPushButton::clicked, this, &Timeline::removeTrack);
    tbl->addWidget(delBtn);

    tbl->addStretch();

    auto *zoomOutBtn = new QPushButton(QString::fromUtf8("\xe2\x88\x92"));  // −
    zoomOutBtn->setFixedSize(24, 24);
    zoomOutBtn->setStyleSheet(btnStyle);
    connect(zoomOutBtn, &QPushButton::clicked, this, [this]() { zoomOut(); });
    tbl->addWidget(zoomOutBtn);

    auto *zoomInBtn = new QPushButton("+");
    zoomInBtn->setFixedSize(24, 24);
    zoomInBtn->setStyleSheet(btnStyle);
    connect(zoomInBtn, &QPushButton::clicked, this, [this]() { zoomIn(); });
    tbl->addWidget(zoomInBtn);

    // ---------- Root layout ----------
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(tb);
    root->addWidget(m_rulerView);
    root->addWidget(m_trackView, 1);

    // Wheel event filter for Ctrl+zoom
    m_rulerView->viewport()->installEventFilter(this);
    m_trackView->viewport()->installEventFilter(this);

    // Default: 3 tracks
    for (int i = 1; i <= 3; i++) addTrack();
}

inline void Timeline::addBlockToFirstTrack(const QString &name)
{
    TrackWidget *t = m_composition->track(0);
    if (t) {
        t->addBlock(name);
        updateSceneRects();
    }
}

inline bool Timeline::hasBlockSelected() const
{
    for (auto *track : m_composition->tracks())
        for (auto *blk : track->blocks())
            if (blk->isSelected()) return true;
    return false;
}

inline void Timeline::deleteSelectedBlock()
{
    for (auto *track : m_composition->tracks()) {
        for (auto *blk : track->blocks()) {
            if (blk->isSelected()) {
                track->deleteBlock(blk);
                updateSceneRects();
                return;  // delete one at a time
            }
        }
    }
}

inline void Timeline::addTrack()
{
    auto *t = new TrackWidget(
        QString::fromUtf8("轨道 ") + QString::number(++m_trackCount), m_composition);
    m_composition->addTrack(t);
    updateSceneRects();
    updatePlayheadLine();
}

inline void Timeline::removeTrack()
{
    if (m_composition->trackCount() <= 1) return;
    m_composition->removeTrack(m_composition->trackCount() - 1);
    m_trackCount--;
    updateSceneRects();
    updatePlayheadLine();
}

inline void Timeline::zoomIn()
{
    m_rulerView->zoomIn();
    m_trackView->zoomIn();
    m_rulerView->horizontalScrollBar()->setValue(
        m_trackView->horizontalScrollBar()->value());
}

inline void Timeline::zoomOut()
{
    m_rulerView->zoomOut();
    m_trackView->zoomOut();
    m_rulerView->horizontalScrollBar()->setValue(
        m_trackView->horizontalScrollBar()->value());
}

inline void Timeline::play()
{
    if (!m_playTimer) return;
    if (!m_playTimer->isActive()) {
        // Resume from where the line currently is
        m_playStartX = m_playPos;
        m_playClock.start();
        updatePlayheadLine();
        if (auto *sb = m_trackView->horizontalScrollBar()) {
            if (m_playPos < sb->value() + 10)
                sb->setValue(qMax(0, int(m_playPos - 40)));
        }
        m_playTimer->start();
    }
}

inline void Timeline::pause()
{
    if (m_playTimer) {
        m_playTimer->stop();
        m_playStartX = m_playPos;
    }
}

inline void Timeline::resetPlayhead()
{
    m_playPos = TRACK_LABEL_W;
    m_playStartX = TRACK_LABEL_W;
    updatePlayheadLine();
    if (auto *sb = m_trackView->horizontalScrollBar())
        sb->setValue(0);
}

inline void Timeline::seekTo(qreal sceneX)
{
    bool wasPlaying = m_playTimer && m_playTimer->isActive();
    if (wasPlaying) m_playTimer->stop();
    m_playPos = sceneX;
    m_playStartX = sceneX;
    updatePlayheadLine();
    if (wasPlaying) { m_playClock.start(); m_playTimer->start(); }
}

inline void Timeline::stepPrev()
{
    pause();
    m_playPos = qMax(qreal(TRACK_LABEL_W), m_playPos - PX_PER_SEC);
    updatePlayheadLine();
    if (auto *sb = m_trackView->horizontalScrollBar())
        sb->setValue(qMax(0, int(m_playPos - 100)));
}

inline void Timeline::stepNext()
{
    pause();
    m_playPos += PX_PER_SEC;
    updatePlayheadLine();
    if (auto *sb = m_trackView->horizontalScrollBar())
        sb->setValue(int(m_playPos - 100));
}

inline void Timeline::updatePlayheadLine()
{
    if (!m_playhead) return;
    qreal h = m_composition ? m_composition->size().height() : 300;
    m_playhead->setLine(m_playPos, 0, m_playPos, h);
}

inline bool Timeline::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent *>(event);
        if (we->modifiers() & Qt::ControlModifier) {
            qreal factor = qPow(1.2, we->angleDelta().y() / 240.0);
            bool ok1 = m_rulerView->scaleView(factor);
            bool ok2 = m_trackView->scaleView(factor);
            if (ok1 || ok2) {
                m_rulerView->horizontalScrollBar()->setValue(
                    m_trackView->horizontalScrollBar()->value());
            }
            return true;
        }
        if (watched == m_rulerView->viewport()) {
            QCoreApplication::sendEvent(m_trackView->viewport(), event);
            return true;
        }
    }
    // Click/drag on ruler or track → seek playhead
    if (watched == m_rulerView->viewport() || watched == m_trackView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->buttons() & Qt::LeftButton) {
                QGraphicsView *gv = (watched == m_rulerView->viewport())
                    ? static_cast<QGraphicsView *>(m_rulerView)
                    : static_cast<QGraphicsView *>(m_trackView);
                QPointF scenePt = gv->mapToScene(me->pos());
                seekTo(qMax(qreal(TRACK_LABEL_W), scenePt.x()));
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

inline void Timeline::updateSceneRects()
{
    // Calculate required width: rightmost block edge + margin
    qreal maxX = TRACK_LABEL_W + 3000.0;  // minimum ~50 s
    for (auto *track : m_composition->tracks()) {
        for (auto *blk : track->blocks()) {
            qreal right = TRACK_LABEL_W + (blk->posSec() + blk->durSec()) * PX_PER_SEC + 100;
            if (right > maxX) maxX = right;
        }
    }
    ensureSceneWidth(maxX);
}

inline void Timeline::ensureSceneWidth(qreal w)
{
    // Ruler
    m_ruler->resize(w, RULER_H);
    m_rulerScene->setSceneRect(0, 0, w, RULER_H);

    // Composition — must explicitly resize so children expand to full width
    qreal compH = m_composition->trackCount() * (TRACK_H + 2);
    m_composition->resize(w, qMax(compH, 1.0));
    m_trackScene->setSceneRect(0, 0, w, qMax(compH, 1.0));
}

#endif // TIMELINE_H
