#include "fixtureitem.h"
#include "Fixture.h"

#include <QGraphicsSceneMouseEvent>

FixtureItem::FixtureItem(Fixture *fixture, QGraphicsItem *parent)
    : QGraphicsEllipseItem(-20, -20, 40, 40, parent)  // 直径 40px
    , m_fixture(fixture)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setPen(QPen(Qt::gray, 2));
    setBrush(QBrush(Qt::white));
    setZValue(1);
}

void FixtureItem::setSelected(bool selected)
{
    if (selected)
        setPen(QPen(Qt::yellow, 3));
    else
        setPen(QPen(Qt::gray, 2));
    QGraphicsEllipseItem::setSelected(selected);
}

void FixtureItem::updateFromData()
{
    // 始终亮白，不随通道值变化
    setBrush(QBrush(Qt::white));
}

void FixtureItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsEllipseItem::mousePressEvent(event);
    // 点击后通知 MainWindow（通过 scene 的 focusItemChanged 信号）
}
