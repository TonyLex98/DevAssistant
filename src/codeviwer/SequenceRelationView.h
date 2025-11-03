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
#include <QDebug>
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
    void rebuildGraphForTemplate(const QString &templateDirPath); // Rebuild graph for specific template
    void savePositions();  // Public slot for saving positions when nodes move
    void saveLayout();     // Public slot for saving layout when closing
    void restoreLayout();  // Public slot for restoring layout when opening

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
    
    // Position storage
    QMap<QString, QPointF> m_nodePositions;  // sequence name -> position
    
    // Template filtering
    QString m_currentTemplateFilter;  // Currently active template filter (empty = no filter)
    
    // Helper method to clear the scene while preserving positions if needed
    void clearScenePreservingPositions();
    
    void clearScene();
    void buildScene();
    void buildSceneForTemplate(const QString &templateDirPath); // Build scene for specific template
    void findSequenceConnections();
    void findSequenceConnectionsForTemplate(const QString &templateDirPath); // Find connections for template
    
    // Helper methods
    void addNode(const QString &name, const QString &description);
    void addEdge(const QString &from, const QString &to);
    void positionNodes();
    
    // Position saving/loading
    void saveNodePositions();
    void loadNodePositions();
    
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

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

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
    QPointF getSideCenterPoint(SequenceNodeItem *node, const QPointF &targetPoint) const;
    
    SequenceNodeItem* source() const { return m_source; }
    SequenceNodeItem* dest() const { return m_dest; }
    
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    SequenceNodeItem *m_source;
    SequenceNodeItem *m_dest;
    QPointF m_sourcePoint;
    QPointF m_destPoint;
};