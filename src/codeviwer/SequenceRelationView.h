#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QWheelEvent>
#include <QAction>
#include <QMenu>
#include <QInputDialog>
#include <QContextMenuEvent>
#include <QMap>
#include <QVector>
#include <QPointF>
#include "PromptSequence.h"
#include "PromptSequenceManager.h"

class SequenceNodeItem;
class SequenceEdgeItem;

class SequenceRelationView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit SequenceRelationView(QWidget *parent = nullptr);
    void setSequenceManager(PromptSequenceManager *seqMgr);
    void setCurrentSequence(const QString &sequenceName);  // Highlight the current sequence

public slots:
    void rebuildGraph();

signals:
    void sequenceSelected(const QString &sequenceName);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QGraphicsScene *m_scene = nullptr;
    PromptSequenceManager *m_seqMgr = nullptr;
    
    // Node and edge tracking
    QMap<QString, SequenceNodeItem*> m_nodeItems;  // sequence name -> item
    QVector<SequenceEdgeItem*> m_edgeItems;
    
    void clearScene();
    void buildScene();
    void findSequenceConnections();
    
    // Helper methods
    void addNode(const QString &name, const QString &description);
    void addEdge(const QString &from, const QString &to);
    void positionNodes();
    
    // Context menu
    void showContextMenu(const QPoint &pos);
};

// Node item representing a sequence
class SequenceNodeItem : public QGraphicsItem
{
public:
    SequenceNodeItem(const QString &name, const QString &description, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    
    QString name() const { return m_name; }
    QString description() const { return m_description; }
    
    void setHighlighted(bool highlighted);
    
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

private:
    QString m_name;
    QString m_description;
    QRectF m_rect;
    bool m_highlighted;
};

// Edge item representing the connection between sequences
class SequenceEdgeItem : public QGraphicsItem
{
public:
    SequenceEdgeItem(SequenceNodeItem *source, SequenceNodeItem *dest, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    
    void updatePosition();
    
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

private:
    SequenceNodeItem *m_source;
    SequenceNodeItem *m_dest;
    QPointF m_sourcePoint;
    QPointF m_destPoint;
};