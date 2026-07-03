/*
 * 自定义 QGraphicsView：支持从灯库拖拽到指定位置放置灯具
 */

#ifndef DROPVIEW_H
#define DROPVIEW_H

#include <QGraphicsView>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>

class DropView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit DropView(QWidget *parent = nullptr) : QGraphicsView(parent)
    {
        setAcceptDrops(true);
    }

signals:
    void fixtureDropped(int libraryIndex, const QPointF &scenePos);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-light-fixture-index"))
            event->acceptProposedAction();
        else
            QGraphicsView::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-light-fixture-index"))
            event->acceptProposedAction();
        else
            QGraphicsView::dragMoveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-light-fixture-index"))
        {
            int idx = event->mimeData()->data("application/x-light-fixture-index").toInt();
            QPointF pos = mapToScene(event->position().toPoint());
            emit fixtureDropped(idx, pos);
            event->acceptProposedAction();
        }
        else
        {
            QGraphicsView::dropEvent(event);
        }
    }
};

#endif // DROPVIEW_H
