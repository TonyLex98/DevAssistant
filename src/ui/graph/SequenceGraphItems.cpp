#include "ui/graph/SequenceGraphItems.h"
#include "ui/graph/SequenceRelationView.h"
#include "core/util/Logging.h"
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLineF>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QVector>
#include <QtMath>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

// Items live in a scene shown by SequenceRelationView; the first view is the one that owns the graph.
SequenceRelationView *relationViewOf(const QGraphicsScene *scene)
{
    if (!scene)
        return nullptr;
    const QList<QGraphicsView*> views = scene->views();
    return views.isEmpty() ? nullptr : qobject_cast<SequenceRelationView*>(views.first());
}

} // namespace

SequenceNodeItem::SequenceNodeItem(const QString &name, const QString &description, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_name(name)
    , m_description(description)
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    if (!m_description.isEmpty()) {
        setToolTip(QString("%1\n%2").arg(m_name, m_description));
    } else {
        setToolTip(m_name);
    }

    // The box is as wide as the longer of the two text lines, so neither gets cut in the middle.
    QFont font;
    font.setPointSize(10);
    font.setBold(true);
    QFontMetrics fm(font);

    int nameWidth = fm.horizontalAdvance(m_name);
    int descWidth = 0;
    if (!m_description.isEmpty()) {
        QFont descFont;
        descFont.setPointSize(8);
        QFontMetrics descFm(descFont);
        descWidth = descFm.horizontalAdvance(m_description);
    }

    int maxWidth = qMax(nameWidth, descWidth);
    int width = qMax(maxWidth + 20, 120);
    int height = 60; // room for the name line and the description line

    m_rect = QRectF(-width/2, -height/2, width, height);
}

QRectF SequenceNodeItem::boundingRect() const
{
    return m_rect;
}

void SequenceNodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QColor fillColor(200, 200, 200);
    if (m_highlighted) {
        fillColor = QColor(144, 238, 144); // light green marks the current sequence
    } else if (m_connectionSource) {
        fillColor = QColor(255, 230, 180);
    } else if (isSelected()) {
        fillColor = QColor(180, 205, 255);
    } else if (m_hovered || m_connectionTargetHover) {
        fillColor = QColor(215, 215, 230);
    }

    QLinearGradient gradient(m_rect.topLeft(), m_rect.bottomRight());
    gradient.setColorAt(0, fillColor.lighter(105));
    gradient.setColorAt(1, fillColor.darker(105));

    painter->setBrush(QBrush(gradient));
    QColor borderColor = m_connectionSource ? QColor(220, 140, 60) : QColor(80, 80, 80);
    QPen pen(borderColor, (m_highlighted || m_connectionSource) ? 3 : (isSelected() ? 3 : 2));
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->drawRoundedRect(m_rect, 10, 10);

    QFont font;
    font.setPointSize(10);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(QPen(QColor(0, 0, 0), 1));

    QRectF nameRect = QRectF(m_rect.x() + 10, m_rect.y() + 10, m_rect.width() - 20, 20);
    painter->drawText(nameRect, Qt::AlignCenter, m_name);

    if (!m_description.isEmpty()) {
        QFont descFont;
        descFont.setPointSize(8);
        descFont.setBold(false);
        painter->setFont(descFont);

        QRectF descRect = QRectF(m_rect.x() + 10, m_rect.y() + 30, m_rect.width() - 20, 20);
        QString elidedDesc = painter->fontMetrics().elidedText(m_description, Qt::ElideRight, descRect.width());
        painter->drawText(descRect, Qt::AlignCenter, elidedDesc);
    }
}

void SequenceNodeItem::setHighlighted(bool highlighted)
{
    m_highlighted = highlighted;
    update();
}

void SequenceNodeItem::setConnectionSource(bool on)
{
    if (m_connectionSource != on) {
        m_connectionSource = on;
        update();
    }
}

void SequenceNodeItem::setConnectionTargetHover(bool on)
{
    if (m_connectionTargetHover != on) {
        m_connectionTargetHover = on;
        update();
    }
}

QVariant SequenceNodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change != ItemPositionHasChanged)
        return QGraphicsItem::itemChange(change, value);

    qCDebug(logViewer) << "[SequenceNodeItem]" << m_name << "position changed to:" << value.toPointF();

    QGraphicsScene *currentScene = scene();
    if (!currentScene) {
        qCDebug(logViewer) << "[SequenceNodeItem] No scene available, skipping edge updates";
        return QGraphicsItem::itemChange(change, value);
    }

    // Asking the scene for nearby items is cheaper than walking every link; the view
    // then redraws the links it tracks for this node, including those that no longer overlap.
    const QList<QGraphicsItem*> overlappingItems = currentScene->items(sceneBoundingRect(), Qt::IntersectsItemShape, Qt::AscendingOrder);
    for (QGraphicsItem *item : overlappingItems) {
        if (item && item->type() == SequenceEdgeItem::Type) {
            SequenceEdgeItem *edge = static_cast<SequenceEdgeItem*>(item);
            if (edge->source() == this || edge->dest() == this) {
                edge->updatePosition();
            }
        }
    }

    const QList<QGraphicsView*> views = currentScene->views();
    if (views.isEmpty()) {
        qCDebug(logViewer) << "[SequenceNodeItem] No views found for scene";
    } else if (!views.first()) {
        qCDebug(logViewer) << "[SequenceNodeItem] View is null, cannot cast";
    } else if (SequenceRelationView *relationView = qobject_cast<SequenceRelationView*>(views.first())) {
        relationView->updateEdgesForNode(this);
        qCDebug(logViewer) << "[SequenceNodeItem] Calling savePositions for" << m_name;
        // savePositions() also writes them to the open template, so every move is persisted.
        relationView->savePositions();
    }
    return QGraphicsItem::itemChange(change, value);
}

void SequenceNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    // While a link is being drawn, the hovered box shows that it can take the link.
    SequenceRelationView *view = relationViewOf(scene());
    if (view && view->isConnecting() && view->connectionSource() != this) {
        m_connectionTargetHover = true;
    }
    update();
}

void SequenceNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    m_connectionTargetHover = false;
    update();
}

SequenceEdgeItem::SequenceEdgeItem(SequenceNodeItem *source, SequenceNodeItem *dest, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_source(source)
    , m_dest(dest)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setZValue(-1); // links go under the boxes so they never hide a name
    updatePosition();
}

QRectF SequenceEdgeItem::boundingRect() const
{
    // Extra room for the pen width and the arrowhead.
    qreal extra = 5;
    QRectF bounds(QPointF(std::min(m_sourcePoint.x(), m_destPoint.x()),
                          std::min(m_sourcePoint.y(), m_destPoint.y())),
                  QPointF(std::max(m_sourcePoint.x(), m_destPoint.x()),
                          std::max(m_sourcePoint.y(), m_destPoint.y())));
    return bounds.adjusted(-extra, -extra, extra, extra);
}

void SequenceEdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!m_source || !m_dest)
        return;

    QLineF line(m_sourcePoint, m_destPoint);
    if (qFuzzyCompare(line.length(), qreal(0.)))
        return;

    QPen linePen(QColor(60, 60, 60), 2);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(linePen);
    painter->drawLine(line);

    // The arrowhead sits at the destination and points along the line.
    double angle = std::atan2(line.dy(), line.dx());
    QPointF arrowTip = m_destPoint;
    // Short links get a smaller head so it does not cover the whole line.
    qreal arrowSize = qMin<qreal>(12.0, line.length() / 4);
    QPointF arrowP1 = arrowTip - QPointF(arrowSize * qCos(angle - M_PI / 6), arrowSize * qSin(angle - M_PI / 6));
    QPointF arrowP2 = arrowTip - QPointF(arrowSize * qCos(angle + M_PI / 6), arrowSize * qSin(angle + M_PI / 6));

    painter->setBrush(QColor(100, 100, 100));
    QPolygonF arrowHead;
    arrowHead << arrowTip << arrowP1 << arrowP2;
    painter->drawPolygon(arrowHead);
}

void SequenceEdgeItem::updatePosition()
{
    if (!m_source || !m_dest)
        return;

    // A node may already be gone while the scene is being rebuilt.
    QGraphicsScene *currentScene = scene();
    if (!currentScene || !currentScene->items().contains(m_source) || !currentScene->items().contains(m_dest)) {
        return;
    }

    QPointF sourceCenterScene = m_source->sceneBoundingRect().center();
    QPointF destCenterScene = m_dest->sceneBoundingRect().center();

    QPointF sourceScenePoint = getSideCenterPoint(m_source, destCenterScene);
    QPointF destScenePoint = getSideCenterPoint(m_dest, sourceCenterScene);

    // Qt must be told before the geometry changes, or it repaints the old area.
    prepareGeometryChange();
    m_sourcePoint = mapFromScene(sourceScenePoint);
    m_destPoint = mapFromScene(destScenePoint);
}

// Where the line from the node centre towards targetPoint leaves the node's box,
// so the arrow starts and ends on the border instead of under the text.
QPointF SequenceEdgeItem::getSideCenterPoint(SequenceNodeItem *node, const QPointF &targetPoint) const
{
    if (!node)
        return QPointF();

    QRectF rect = node->sceneBoundingRect();
    QPointF center = rect.center();
    QLineF pointer(center, targetPoint);

    QVector<QLineF> edges;
    edges << QLineF(rect.topLeft(), rect.topRight())
          << QLineF(rect.bottomLeft(), rect.bottomRight())
          << QLineF(rect.topLeft(), rect.bottomLeft())
          << QLineF(rect.topRight(), rect.bottomRight());

    QPointF intersection;
    for (const QLineF &edge : edges) {
        if (pointer.intersects(edge, &intersection) == QLineF::BoundedIntersection) {
            return intersection;
        }
    }

    // The target is inside the box (overlapping nodes), so there is no border crossing.
    return center;
}
