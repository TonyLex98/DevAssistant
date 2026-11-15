#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QToolBar>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <QWheelEvent>
#include <QAction>
#include <QMenu>
#include <QInputDialog>
#include <QContextMenuEvent>
#include <QSet>
#include "ui/graph/GraphItems.h"
#include "ui/graph/GraphLayout.h"
#include "core/code/ProjectIndexer.h"
#include "core/code/CodePatcher.h"
#include <cmath>

class ProjectRelationView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ProjectRelationView(QWidget *parent=nullptr);
    void setProjectRoot(const QString &root);

signals:
    void openInEditor(const QString &file, int line);

public slots:
    void rebuildGraph();
    void validateLostEdges();

protected:
    void wheelEvent(QWheelEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

private slots:
    void onGraphReady(const SymbolGraph &g);

private:
    QGraphicsScene *m_scene=nullptr;
    QToolBar *m_toolbar=nullptr;
    GraphLayout *m_layout=nullptr;
    ProjectIndexer *m_indexer=nullptr;
    CodePatcher *m_patcher=nullptr;
    QString m_root;
    SymbolGraph m_graph; // last indexed

    // items
    QHash<QString, ClassItem*> m_classItems; // class sid -> item
    QHash<QString, MethodChip*> m_methodItems; // method sid -> item
    QList<EdgeItem*> m_edgeItems;

    // persistence
    QHash<QString, QPointF> m_savedClassPos;
    QHash<QString, QHash<QString, QPointF>> m_savedMethodLocalPos; // classSid -> (methodSid -> pos)
    QString layoutPath() const;
    void saveLayout() const;
    void loadLayout();
    void applySavedLayout();

    // helpers
    void clearScene();
    void buildSceneFromGraph(const SymbolGraph &g);
    QPointF anchorForSid(const QString &sid) const;
    QString edgeKey(const EdgeData &e) const { return e.fromSid+"->"+e.toSid+"|"+QString::number((int)e.kind); }
    void refreshEdges();
    void showContextMenuForItem(QGraphicsItem* item, const QPoint &globalPos);



    // diff
    QSet<QString> m_prevEdgeKeys;
};
