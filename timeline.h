#ifndef TIMELINE_H
#define TIMELINE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QRegularExpression>
#include <QPainter>
#include <QColor>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QList>
#include <QDrag>

// ===== 可拖放+缩放的时间块 =====
class TimelineBlock : public QFrame
{
    Q_OBJECT
public:
    explicit TimelineBlock(const QString &fixtureName, double startSec, double durationSec,
                           QWidget *parent = nullptr)
        : QFrame(parent), m_name(fixtureName), m_start(startSec), m_duration(durationSec)
    {
        setFixedHeight(32);
        setAcceptDrops(true);
        int hue = qHash(fixtureName) % 360;
        m_color = QColor::fromHsv(hue, 140, 210);
        updateStyle();
        auto *hl = new QHBoxLayout(this);
        hl->setContentsMargins(8,0,8,0);
        m_label = new QLabel(fixtureName);
        m_label->setStyleSheet("color:#222;font-size:10px;border:none;background:transparent");
        hl->addWidget(m_label); hl->addStretch();

        // 左右拖拽手柄
        m_leftHandle = new QFrame(this); m_leftHandle->setFixedWidth(6);
        m_leftHandle->setCursor(Qt::SizeHorCursor);
        m_leftHandle->setStyleSheet("background:transparent");
        m_leftHandle->installEventFilter(this);
        m_rightHandle = new QFrame(this); m_rightHandle->setFixedWidth(6);
        m_rightHandle->setCursor(Qt::SizeHorCursor);
        m_rightHandle->setStyleSheet("background:transparent");
        m_rightHandle->installEventFilter(this);
    }

    void updateGeometry()
    {
        int x = static_cast<int>(m_start * 60); // 60px/s
        int w = qMax(static_cast<int>(m_duration * 60), 30);
        setGeometry(x, 2, w, 32);
        m_leftHandle->setGeometry(0, 0, 6, 32);
        m_rightHandle->setGeometry(w - 6, 0, 6, 32);
    }

    double startSec()  const { return m_start; }
    double durationSec() const { return m_duration; }
    QString fixtureName() const { return m_name; }

    void setStart(double s) { m_start = s; updateStyle(); }
    void setDuration(double d) { m_duration = qMax(d, 0.5); updateStyle(); }

protected:
    bool eventFilter(QObject *obj, QEvent *e) override
    {
        if (e->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true; m_dragStart = me->globalPosition().toPoint();
                m_isResize = (obj == m_leftHandle || obj == m_rightHandle);
                m_resizeLeft = (obj == m_leftHandle);
                m_origStart = m_start; m_origDur = m_duration;
                return true;
            }
        }
        return QFrame::eventFilter(obj, e);
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!m_dragging) return;
        int dx = e->globalPosition().toPoint().x() - m_dragStart.x();
        if (m_isResize) {
            double sec = dx / 60.0;
            if (m_resizeLeft) {
                double newStart = m_origStart + sec;
                double newDur = m_origDur - sec;
                if (newDur >= 0.5 && newStart >= 0) {
                    m_start = newStart; m_duration = newDur;
                }
            } else {
                m_duration = qMax(m_origDur + sec, 0.5);
            }
        } else {
            m_start = m_origStart + dx / 60.0;
            if (m_start < 0) m_start = 0;
        }
        updateStyle();
        updateGeometry();
        QFrame::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        if (m_dragging && !m_isResize) {
            m_dragging = false;
            // 检查是否拖到了另一个轨道：启动 Drag 让 TimelineTrack::dropEvent 处理
            auto *drag = new QDrag(this);
            auto *mime = new QMimeData;
            mime->setText(m_name);
            mime->setData("x-timeline-move", QByteArray::number(m_start) + "," + QByteArray::number(m_duration));
            drag->setMimeData(mime);
            setVisible(false); // 隐藏自己，等 drop 决定
            Qt::DropAction act = drag->exec(Qt::MoveAction);
            if (act != Qt::MoveAction) {
                setVisible(true); // drop 没成功，恢复
            } else {
                deleteLater(); // 已经移到新轨道，删除旧块
            }
        }
        m_dragging = false;
    }

    void updateStyle()
    {
        setStyleSheet(QString(
            "TimelineBlock{background:%1;border-radius:4px;border:1px solid %2}"
            "TimelineBlock:hover{background:%3}")
            .arg(m_color.lighter(145).name())
            .arg(m_color.name())
            .arg(m_color.lighter(160).name()));
        if (m_label) m_label->setText(QString("%1  %2s").arg(m_name).arg(m_duration, 0, 'f', 1));
    }

    QString m_name;
    double m_start, m_duration;
    QColor m_color;
    QLabel *m_label;
    QFrame *m_leftHandle, *m_rightHandle;
    bool m_dragging = false, m_isResize = false, m_resizeLeft = false;
    QPoint m_dragStart;
    double m_origStart, m_origDur;
};

// ===== 轨道 =====
class TimelineTrack : public QFrame
{
    Q_OBJECT
public:
    explicit TimelineTrack(const QString &name, QWidget *parent = nullptr) : QFrame(parent)
    {
        setFixedHeight(44);
        setAcceptDrops(true);
        setStyleSheet("background:#fff;border-bottom:1px solid #ddd");
        auto *hl = new QHBoxLayout(this);
        hl->setContentsMargins(4,2,4,2);
        auto *label = new QLabel(name);
        label->setFixedWidth(100);
        label->setStyleSheet("color:#555;font-size:11px;border:none;background:transparent");
        hl->addWidget(label);
        // 块容器 — 绝对定位
        m_blockArea = new QWidget;
        m_blockArea->setStyleSheet("background:transparent");
        hl->addWidget(m_blockArea, 1);
    }

    void addBlock(const QString &fixtureName, double startSec = 0, double durationSec = 5)
    {
        auto *block = new TimelineBlock(fixtureName, startSec, durationSec, m_blockArea);
        block->updateGeometry();
        block->show();
        m_blocks << block;
    }

    QList<TimelineBlock *> blocks() const { return m_blocks; }
    QWidget *blockArea() const { return m_blockArea; }

protected:
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        if (e->mimeData()->hasText()) e->acceptProposedAction();
    }
    void dropEvent(QDropEvent *e) override
    {
        if (!e->mimeData()->hasText()) return;
        QString name = e->mimeData()->text();
        double startSec = e->position().toPoint().x() / 60.0;
        double dur = 5;
        // 如果是轨道间移动，保持原时长
        if (e->mimeData()->hasFormat("x-timeline-move")) {
            auto parts = QString::fromUtf8(e->mimeData()->data("x-timeline-move")).split(",");
            if (parts.size() >= 2) dur = parts[1].toDouble();
        }
        addBlock(name, startSec, dur);
        e->acceptProposedAction();
    }

private:
    QWidget *m_blockArea;
    QList<TimelineBlock *> m_blocks;
};

// ===== 时间线 =====
class Timeline : public QWidget
{
    Q_OBJECT
public:
    explicit Timeline(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0,0,0,0); root->setSpacing(0);
        auto *topBar = new QHBoxLayout;
        auto *title = new QLabel("时间线");
        title->setStyleSheet("color:#333;font-weight:bold;font-size:13px;padding:4px 8px;border:none;background:#f0f0f0");
        topBar->addWidget(title);
        auto *addBtn = new QPushButton("+ 轨道");
        addBtn->setStyleSheet("background:#e0e0ff;color:#224;border:1px solid #aab;border-radius:3px;padding:2px 8px;font-size:11px");
        connect(addBtn, &QPushButton::clicked, this, &Timeline::addTrack);
        topBar->addWidget(addBtn);
        auto *delBtn = new QPushButton("- 轨道");
        delBtn->setStyleSheet("background:#ffe0e0;color:#422;border:1px solid #baa;border-radius:3px;padding:2px 8px;font-size:11px");
        connect(delBtn, &QPushButton::clicked, this, &Timeline::removeTrack);
        topBar->addWidget(delBtn);
        m_countLabel = new QLabel("3 条轨道");
        m_countLabel->setStyleSheet("color:#888;font-size:11px;border:none;background:transparent;padding:4px");
        topBar->addWidget(m_countLabel); topBar->addStretch();
        auto *topFrame = new QFrame;
        topFrame->setStyleSheet("background:#f0f0f0;border-bottom:1px solid #ccc");
        topFrame->setLayout(topBar); root->addWidget(topFrame);

        m_trackContainer = new QWidget;
        m_trackLayout = new QVBoxLayout(m_trackContainer);
        m_trackLayout->setContentsMargins(0,0,0,0); m_trackLayout->setSpacing(0);
        m_trackContainer->setStyleSheet("background:#f5f5f5");
        for (int i = 1; i <= 3; i++) addTrack();

        auto *scroll = new QScrollArea; scroll->setWidgetResizable(true);
        scroll->setWidget(m_trackContainer); root->addWidget(scroll, 1);

        auto *ruler = new QFrame;
        ruler->setFixedHeight(24);
        ruler->setStyleSheet("background:#e8e8e8;border-top:1px solid #ccc");
        for (int i = 0; i <= 60; i++) {
            auto *tick = new QLabel(QString("%1s").arg(i), ruler);
            tick->setStyleSheet("color:#999;font-size:9px;border:none;background:transparent");
            tick->setFixedWidth(30);
            tick->move(104 + i * 60 - 15, 2); // 居中在刻度上
            if (i % 5 != 0) tick->setText(""); // 整 5s 才显示文字
        }
        root->addWidget(ruler);
    }

    int trackCount() const { return m_trackCount; }

    void addBlockToFirstTrack(const QString &fixtureName) {
        for (int i = 0; i < m_trackLayout->count(); i++) {
            auto *track = qobject_cast<TimelineTrack *>(m_trackLayout->itemAt(i)->widget());
            if (track) { track->addBlock(fixtureName); return; }
        }
    }

protected:
    void dragEnterEvent(QDragEnterEvent *e) override {
        if (e->mimeData()->hasText()) e->acceptProposedAction();
    }
    void dropEvent(QDropEvent *e) override {
        if (!e->mimeData()->hasText()) return;
        QString name = e->mimeData()->text();
        // 找到鼠标所在的轨道
        QPoint localPos = m_trackContainer->mapFrom(this, e->position().toPoint());
        for (int i = 0; i < m_trackLayout->count(); i++) {
            auto *w = m_trackLayout->itemAt(i)->widget();
            if (w && w->geometry().contains(localPos)) {
                auto *track = qobject_cast<TimelineTrack *>(w);
                if (track) {
                    double startSec = (e->position().toPoint().x() - 104) / 60.0;
                    if (startSec < 0) startSec = 0;
                    double dur = 5;
                    if (e->mimeData()->hasFormat("x-timeline-move")) {
                        auto parts = QString::fromUtf8(e->mimeData()->data("x-timeline-move")).split(",");
                        if (parts.size() >= 2) dur = parts[1].toDouble();
                    }
                    track->addBlock(name, startSec, dur);
                    e->acceptProposedAction();
                    return;
                }
            }
        }
    }

private slots:
    void addTrack() {
        m_trackCount++;
        m_trackLayout->addWidget(new TimelineTrack(QString("轨道 %1").arg(m_trackCount)));
        m_countLabel->setText(QString("%1 条轨道").arg(m_trackCount));
    }
    void removeTrack() {
        if (m_trackCount <= 1) return;
        auto *it = m_trackLayout->takeAt(m_trackLayout->count()-1);
        if (it->widget()) delete it->widget(); delete it;
        m_trackCount--; m_countLabel->setText(QString("%1 条轨道").arg(m_trackCount));
    }

private:
    QWidget *m_trackContainer;
    QVBoxLayout *m_trackLayout;
    QLabel *m_countLabel;
    int m_trackCount = 0;
};

#endif
