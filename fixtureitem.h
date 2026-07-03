/*
 * 2D 视图中的灯具图形（白色圆形）
 */

#ifndef FIXTUREITEM_H
#define FIXTUREITEM_H

#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>

class Fixture;

class FixtureItem : public QGraphicsEllipseItem
{
public:
    explicit FixtureItem(Fixture *fixture, QGraphicsItem *parent = nullptr);

    Fixture *fixture() const { return m_fixture; }

    void setSelected(bool selected);
    void updateFromData();  // dimmer → 白色亮度

    enum { Type = QGraphicsItem::UserType + 1 };
    int type() const override { return Type; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    Fixture *m_fixture;
    QColor   m_baseColor = Qt::white;
};

#endif // FIXTUREITEM_H
