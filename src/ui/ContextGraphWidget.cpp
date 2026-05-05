#include "ContextGraphWidget.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QVBoxLayout>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QQueue>
#include <QPainter>

namespace {
constexpr qreal kNodeWidth = 260.0;
constexpr qreal kNodeHeight = 110.0;
constexpr qreal kXGap = 90.0;
constexpr qreal kYGap = 36.0;
}

ContextGraphWidget::ContextGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setBackgroundBrush(QColor(246, 252, 250));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    setLayout(layout);
}

void ContextGraphWidget::setGraphData(const QList<ContextGraphNodeInfo>& nodes, const QString& activeNodeId)
{
    m_nodes = nodes;
    m_activeNodeId = activeNodeId;
    rebuildScene();
}

QColor ContextGraphWidget::colorForCategory(const QString& category) const
{
    const QString c = category.trimmed().toLower();
    if (c == QStringLiteral("project"))
        return QColor(221, 240, 255);
    if (c == QStringLiteral("news"))
        return QColor(255, 236, 221);
    if (c == QStringLiteral("search"))
        return QColor(233, 251, 229);
    if (c == QStringLiteral("system"))
        return QColor(238, 236, 255);
    return QColor(240, 240, 240);
}

QString ContextGraphWidget::buildNodeLabel(const ContextGraphNodeInfo& node) const
{
    QStringList lines;
    lines << QStringLiteral("[%1] %2").arg(node.category, node.title);
    if (!node.summary.trimmed().isEmpty())
        lines << node.summary.simplified();
    lines << QStringLiteral("Actions: %1").arg(node.actions.size());
    if (node.milestone)
        lines << QStringLiteral("Milestone");
    return lines.join(QStringLiteral("\n"));
}

void ContextGraphWidget::rebuildScene()
{
    m_scene->clear();
    m_positions.clear();

    if (m_nodes.isEmpty()) {
        auto* empty = m_scene->addText(tr("Context graph is empty."));
        empty->setDefaultTextColor(QColor(90, 90, 90));
        empty->setPos(12, 10);
        return;
    }

    QMap<QString, ContextGraphNodeInfo> byId;
    QMap<QString, QList<QString>> children;
    QMap<QString, QString> parentById;
    for (const auto& node : m_nodes) {
        byId[node.id] = node;
        parentById[node.id] = node.parentId;
    }
    for (const auto& node : m_nodes) {
        if (!node.parentId.isEmpty() && byId.contains(node.parentId))
            children[node.parentId].append(node.id);
    }

    QStringList roots;
    for (const auto& node : m_nodes) {
        if (node.parentId.isEmpty() || !byId.contains(node.parentId))
            roots << node.id;
    }
    roots.removeDuplicates();
    std::sort(roots.begin(), roots.end());

    QMap<QString, int> depthById;
    QQueue<QString> queue;
    for (const QString& root : roots) {
        depthById[root] = 0;
        queue.enqueue(root);
    }
    while (!queue.isEmpty()) {
        const QString id = queue.dequeue();
        const int d = depthById.value(id, 0);
        for (const QString& ch : children.value(id)) {
            if (!depthById.contains(ch) || depthById.value(ch) > d + 1) {
                depthById[ch] = d + 1;
                queue.enqueue(ch);
            }
        }
    }

    QMap<int, QList<QString>> levels;
    for (const auto& node : m_nodes) {
        const int d = depthById.value(node.id, 0);
        levels[d].append(node.id);
    }
    for (auto it = levels.begin(); it != levels.end(); ++it) {
        auto list = it.value();
        std::sort(list.begin(), list.end(), [&](const QString& a, const QString& b) {
            const auto na = byId.value(a);
            const auto nb = byId.value(b);
            if (na.updatedAt.isValid() && nb.updatedAt.isValid() && na.updatedAt != nb.updatedAt)
                return na.updatedAt < nb.updatedAt;
            return a < b;
        });
        it.value() = list;
    }

    for (auto it = levels.cbegin(); it != levels.cend(); ++it) {
        const int depth = it.key();
        const QList<QString>& ids = it.value();
        for (int row = 0; row < ids.size(); ++row) {
            const qreal x = 20 + depth * (kNodeWidth + kXGap);
            const qreal y = 20 + row * (kNodeHeight + kYGap);
            m_positions[ids.at(row)] = QPointF(x, y);
        }
    }

    QPen edgePen(QColor(120, 120, 120));
    edgePen.setWidth(2);
    for (const auto& node : m_nodes) {
        if (node.parentId.isEmpty() || !m_positions.contains(node.parentId) || !m_positions.contains(node.id))
            continue;
        const QPointF p1 = m_positions.value(node.parentId) + QPointF(kNodeWidth, kNodeHeight / 2.0);
        const QPointF p2 = m_positions.value(node.id) + QPointF(0.0, kNodeHeight / 2.0);
        m_scene->addLine(QLineF(p1, p2), edgePen);
    }

    for (const auto& node : m_nodes) {
        if (!m_positions.contains(node.id))
            continue;
        const QPointF pos = m_positions.value(node.id);
        const QRectF rect(pos.x(), pos.y(), kNodeWidth, kNodeHeight);
        const bool isActive = (node.id == m_activeNodeId);

        QPen pen(isActive ? QColor(30, 130, 190) : QColor(90, 90, 90));
        pen.setWidth(isActive ? 3 : 1);
        auto* box = m_scene->addRect(rect, pen, QBrush(colorForCategory(node.category)));
        box->setZValue(1);

        auto* text = m_scene->addText(buildNodeLabel(node), QFont(QStringLiteral("Segoe UI"), 8));
        text->setTextWidth(kNodeWidth - 12);
        text->setDefaultTextColor(QColor(20, 20, 20));
        text->setPos(pos + QPointF(6, 5));
        text->setZValue(2);
    }

    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-24, -24, 24, 24));
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}
