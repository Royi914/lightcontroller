/*
 * 2D 视图中的灯具图形（白色圆形）
 */

#ifndef FIXTUREITEM_H
#define FIXTUREITEM_H

#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>

class Fixture;

class FixtureItem : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT
public:
    explicit FixtureItem(Fixture *fixture, QGraphicsItem *parent = nullptr);

    Fixture *fixture() const { return m_fixture; }

    void setSelected(bool selected);
    void updateFromData();  // dimmer → 白色亮度

    enum { Type = QGraphicsItem::UserType + 1 };
    int type() const override { return Type; }

signals:
    void positionChanged(FixtureItem *item);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    Fixture *m_fixture;
    QColor   m_baseColor = Qt::white;
};

#endif // FIXTUREITEM_H
