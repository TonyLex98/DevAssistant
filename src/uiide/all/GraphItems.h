#pragma once
#include <QGraphicsObject>
#include <QGraphicsPathItem>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPointer>
#include <QVariantAnimation>
#include "SymbolModel.h"
#include <QCursor>

class MethodChip;

class ClassItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit ClassItem(const NodeData &data);
    QRectF boundingRect() const override;
    void paint(QPainter* p, const QStyleOptionGraphicsItem* o, QWidget* w) override;

    void setHeader(const QString &h) { m_header = h; update(); }
    QString sid() const { return m_data.sid.id; }
    const NodeData& data() const { return m_data; }

    void addChildChip(MethodChip* chip);
    QPointF chipAnchor(const MethodChip* chip) const; // where edges should attach

    // Persistence
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &o);

signals:
    void positionChanged();
    void heldChanged(bool held);              // <-- ДОБАВИЛИ

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;    // <-- ДОБАВИЛИ
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;  // <-- ДОБАВИЛИ

private:
    NodeData m_data;
    QString m_header;
    QList<MethodChip*> m_children;
    QRectF m_rectCache{0,0,360,120};
    bool m_layoutInitialized=false;
    void ensureInitialLayout();
};

class MethodChip : public QGraphicsObject {
    Q_OBJECT
public:
    explicit MethodChip(const NodeData &data, QGraphicsItem* parent=nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* p, const QStyleOptionGraphicsItem* o, QWidget* w) override;

    QString sid() const { return m_data.sid.id; }
    const NodeData& data() const { return m_data; }

    // Persistence
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &o);

signals:
    void positionChanged();
    void openRequested(const QString &file, int line);
    void heldChanged(bool held);              // <-- ДОБАВИЛИ

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;    // <-- ДОБАВИЛИ
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;  // <-- ДОБАВИЛИ

private:
    NodeData m_data; QRectF m_rect{0,0,240,36};
};

class EdgeItem : public QGraphicsPathItem {
public:
    EdgeItem(const QString &fromSid, const QString &toSid, EdgeKind kind);
    void setEndpoints(const QPointF &a, const QPointF &b);
    void setLabel(const QString &text){ m_label=text; update(); }
    void setOrder(int order){ m_order = order; update(); }
    void setLost(bool lost){ m_lost = lost; update(); }
    void setHighlight(bool on){ m_highlight = on; update(); }  // <-- ДОБАВИЛИ

    enum { Type = UserType + 1 }; int type() const override { return Type; }

protected:
    void paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w) override;

private:
    QString m_from, m_to; EdgeKind m_kind; QString m_label; int m_order=-1; bool m_lost=false;
    QPointF m_a, m_b; QFont m_font;
    bool m_highlight = false;
};
