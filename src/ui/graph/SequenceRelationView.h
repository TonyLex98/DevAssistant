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
#include "core/workflow/PromptSequence.h"
#include "core/workflow/PromptSequenceManager.h"
#include "ui/graph/SequenceGraphItems.h"

class SolutionTemplateManager;
class SolutionTemplate;
struct SequenceGraphEntry;
struct SequenceGraphLink;

// Shows the prompt sequences of a template as boxes joined by arrows. The user can move boxes
// (positions are saved into the template), draw and delete links, and add or remove sequences.
// Which sequences and links exist comes from SequenceGraphModel, the default placement from
// SequenceGraphLayout; this class builds the scene from them and handles the interaction.
class SequenceRelationView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit SequenceRelationView(QWidget *parent = nullptr);
    void setSequenceManager(PromptSequenceManager *seqMgr);
    void setTemplateManager(SolutionTemplateManager *tmplMgr);
    void setCurrentSequence(const QString &sequenceName);  // Highlight the current sequence
    void setCurrentTemplateDir(const QString& templateDirPath); // Template that receives saved positions and edits
    QString currentTemplateDirPath() const { return m_currentTemplateDirPath; }
    SolutionTemplateManager* templateManager() const { return m_templateMgr; }
    void savePositionsToTemplate(const QString& templateDirPath);

public slots:
    void rebuildGraph();
    void rebuildGraphForTemplate(const QString &templateDirPath);
    void savePositions();  // Called whenever a node moves
    void saveLayout();     // Called when the graph is closed
    void restoreLayout();  // Called when the graph is opened
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
    SolutionTemplateManager *m_templateMgr = nullptr;

    QMap<QString, SequenceNodeItem*> m_nodeItems;  // sequence name -> item
    QVector<SequenceEdgeItem*> m_edgeItems;
    QHash<SequenceNodeItem*, QList<SequenceEdgeItem*>> m_nodeEdges;
    QSet<QString> m_edgesSet; // "source->target", to skip duplicate links

    // Survives a rebuild, so a refresh does not throw away where the user put the nodes.
    QMap<QString, QPointF> m_nodePositions;  // sequence name -> position

    QString m_currentTemplateDirPath; // empty when the graph shows the manager's sequences
    QMap<QString, QString> m_sequenceToBaseName; // sequence name -> file base name, used by template links

    bool m_rebuilding = false;  // the scene is being built: node moves are not the user's
    bool m_panning = false;
    bool m_spacePanning = false;
    QPoint m_lastPanPoint;
    QPointF m_lastContextScenePos;
    qreal m_totalScale = 1.0;

    SequenceNodeItem *m_connectSource{nullptr};
    QGraphicsLineItem *m_tempConnectionLine{nullptr};
    bool m_connecting{false};

    // Building the scene
    void clearScenePreservingPositions();
    void addSequenceNodes(const QVector<SequenceGraphEntry> &entries);
    void addLinks(const QVector<SequenceGraphLink> &links);
    void addNode(const QString &name, const QString &description);
    void addEdge(const QString &from, const QString &to);
    bool placeNode(const QString &name, const QPointF &pos);
    void finishRebuild(const QTransform &prevTransform, const QPointF &prevCenter, bool hadViewState);
    void positionNodes();
    void updateAllEdges();
    void removeEdgeItem(const QString &sourceName, const QString &targetName);
    void saveNodePositions();

    // View geometry
    QRectF nodesBounds(bool selectedOnly) const;
    QRectF contentBounds() const;
    void focusOnSelection();
    void expandSceneIfNeeded();
    SequenceNodeItem *nodeAt(const QPointF &scenePos, Qt::SortOrder order,
                             const SequenceNodeItem *ignored = nullptr) const;

    // Editing the template
    void showContextMenu(const QPoint &pos);
    void createSequenceAtContextPos();
    void removeSequenceFromTemplate(const QString &sequenceName);
    QString resolveBaseName(const QString &sequenceName);
    bool addDirectedConnection(const QString &sourceName, const QString &targetName);
    bool removeDirectedConnection(const QString &sourceName, const QString &targetName);
    void startConnectionFrom(SequenceNodeItem *source);
    void updateTempConnectionTo(const QPointF &scenePos);
    void cancelConnection();
};
