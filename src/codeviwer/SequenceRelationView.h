#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QGraphicsSceneHoverEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QAction>
#include <QMenu>
#include <QInputDialog>
#include <QContextMenuEvent>
#include <QMap>
#include <QVector>
#include <QPointF>
#include <QDebug>
#include <QHash>
#include <QSet>
#include <QKeyEvent>
#include <QJsonObject>
#include "PromptSequence.h"
#include "PromptSequenceManager.h"

class SolutionTemplateManager;  // Forward declaration
class SolutionTemplate;

class SequenceNodeItem;
class SequenceEdgeItem;

class SequenceRelationView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit SequenceRelationView(QWidget *parent = nullptr);
    void setSequenceManager(PromptSequenceManager *seqMgr);
    void setTemplateManager(class SolutionTemplateManager *tmplMgr); // Add template manager reference
    void setCurrentSequence(const QString &sequenceName);  // Highlight the current sequence
    void setCurrentTemplateDir(const QString& templateDirPath); // Set current template directory for saving positions
    QString currentTemplateDirPath() const { return m_currentTemplateDirPath; } // Get current template directory path
    SolutionTemplateManager* templateManager() const { return m_templateMgr; } // Get template manager
    void savePositionsToTemplate(const QString& templateDirPath); // Public method for saving positions to template

public slots:
    void rebuildGraph();
    void rebuildGraphForTemplate(const QString &templateDirPath); // Rebuild graph for specific template
    void savePositions();  // Public slot for saving positions when nodes move
    void saveLayout();     // Public slot for saving layout when closing
    void restoreLayout();  // Public slot for restoring layout when opening
    void updateEdgesForNode(SequenceNodeItem *node);
    void fitToContents();
    void resetZoom();
    bool isConnecting() const { return m_connecting; }
    SequenceNodeItem* connectionSource() const { return m_connectSource; }

signals:
    void sequenceSelected(const QString &sequenceName);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    QGraphicsScene *m_scene = nullptr;
    PromptSequenceManager *m_seqMgr = nullptr;
    class SolutionTemplateManager *m_templateMgr = nullptr; // reference to template manager
    
    // Node and edge tracking
    QMap<QString, SequenceNodeItem*> m_nodeItems;  // sequence name -> item
    QVector<SequenceEdgeItem*> m_edgeItems;
    QHash<SequenceNodeItem*, QList<SequenceEdgeItem*>> m_nodeEdges;
    
    // Position storage
    QMap<QString, QPointF> m_nodePositions;  // sequence name -> position
    
    // Template filtering
    QString m_currentTemplateFilter;  // Currently active template filter (empty = no filter)
    QString m_currentTemplateDirPath; // Current template directory path for saving positions
    bool m_panning = false;
    bool m_spacePanning = false;
    QPoint m_lastPanPoint;
    QPointF m_lastContextScenePos;
    qreal m_totalScale = 1.0;
    qreal m_minScale = 0.25;
    qreal m_maxScale = 6.0;
    qreal m_zoomStep = 1.15;
    QSet<QString> m_edgesSet; // source->target
    QMap<QString, QString> m_sequenceToBaseName; // sequence display name -> base filename
    SequenceNodeItem *m_connectSource{nullptr};
    QGraphicsLineItem *m_tempConnectionLine{nullptr};
    bool m_connecting{false};
    
    // Helper method to clear the scene while preserving positions if needed
    void clearScenePreservingPositions();
    
    void clearScene();
    void buildScene();
    void buildSceneForTemplate(const QString &templateDirPath); // Build scene for specific template
    void findSequenceConnections();
    void findSequenceConnectionsForTemplate(const QString &templateDirPath); // Find connections for template
    void findSequenceConnectionsLegacy();
    void findSequenceConnectionsForTemplateLegacy(const QString &templateDirPath);
    
    // Helper methods
    void addNode(const QString &name, const QString &description);
    void addEdge(const QString &from, const QString &to);
    void positionNodes();
    
    // Position saving/loading
    void saveNodePositions();
    void loadNodePositions();
    
    // Context menu
    void showContextMenu(const QPoint &pos);
    QRectF contentBounds() const;
    void focusOnSelection();
    void setEdgesFromTemplate(const QJsonObject &sequenceTargets, const QMap<QString, QString> &baseNameToSequenceNameMap);
    bool loadCurrentTemplate(SolutionTemplate &tmpl) const;
    bool saveTemplate(const SolutionTemplate &tmpl);
    QString resolveBaseName(const QString &sequenceName);
    bool addDirectedConnection(const QString &sourceName, const QString &targetName);
    bool removeDirectedConnection(const QString &sourceName, const QString &targetName);
    void removeEdgeItem(const QString &sourceName, const QString &targetName);
    void removeNodeItem(const QString &sequenceName);
    void startConnectionFrom(SequenceNodeItem *source);
    void updateTempConnectionTo(const QPointF &scenePos);
    void cancelConnection();
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
    bool m_highlighted;
    bool m_hovered{false};
    bool m_connectionSource{false};
    bool m_connectionTargetHover{false};
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
