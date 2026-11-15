#pragma once
#include <QGraphicsItem>
#include <QPointF>
#include <QRectF>
#include <QString>

// A sequence drawn as a rounded box with its name and description.
// Moving it redraws the attached links and stores the new position through the view.
class SequenceNodeItem : public QGraphicsItem
{
public:
    SequenceNodeItem(const QString &name, const QString &description, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    QString name() const { return m_name; }
    QString description() const { return m_description; }

    void setHighlighted(bool highlighted);
    void setConnectionSource(bool on);
    void setConnectionTargetHover(bool on);

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    QString m_name;
    QString m_description;
    QRectF m_rect;
    bool m_highlighted{false};
    bool m_hovered{false};
    bool m_connectionSource{false};
    bool m_connectionTargetHover{false};
};

// A directed link between two sequences: an arrow from the side of one box to the side of the other.
class SequenceEdgeItem : public QGraphicsItem
{
public:
    SequenceEdgeItem(SequenceNodeItem *source, SequenceNodeItem *dest, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    void updatePosition();
    QPointF getSideCenterPoint(SequenceNodeItem *node, const QPointF &targetPoint) const;

    SequenceNodeItem* source() const { return m_source; }
    SequenceNodeItem* dest() const { return m_dest; }

    enum { Type = UserType + 2 };
    int type() const override { return Type; }

private:
    SequenceNodeItem *m_source;
    SequenceNodeItem *m_dest;
    QPointF m_sourcePoint;
    QPointF m_destPoint;
};
