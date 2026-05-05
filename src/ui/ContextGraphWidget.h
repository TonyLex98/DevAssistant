#ifndef CONTEXTGRAPHWIDGET_H
#define CONTEXTGRAPHWIDGET_H

#include <QWidget>
#include <QMap>
#include <QPointF>

#include "../core/ContextManager.h"

class QGraphicsScene;
class QGraphicsView;

class ContextGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ContextGraphWidget(QWidget* parent = nullptr);

    void setGraphData(const QList<ContextGraphNodeInfo>& nodes, const QString& activeNodeId);

private:
    void rebuildScene();
    QColor colorForCategory(const QString& category) const;
    QString buildNodeLabel(const ContextGraphNodeInfo& node) const;

private:
    QGraphicsView* m_view = nullptr;
    QGraphicsScene* m_scene = nullptr;
    QList<ContextGraphNodeInfo> m_nodes;
    QString m_activeNodeId;
    QMap<QString, QPointF> m_positions;
};

#endif // CONTEXTGRAPHWIDGET_H
