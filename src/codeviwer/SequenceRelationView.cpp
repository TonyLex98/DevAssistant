#include "SequenceRelationView.h"
#include <QWheelEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QAction>
#include <QRegularExpression>
#include <QtMath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SequenceNodeItem Implementation
SequenceNodeItem::SequenceNodeItem(const QString &name, const QString &description, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_name(name)
    , m_description(description)
    , m_highlighted(false)
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    
    // Calculate bounding rect based on text
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
    int width = qMax(maxWidth + 20, 120); // minimum width of 120
    int height = 60; // height to accommodate both name and description
    
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

    // Draw background
    if (m_highlighted) {
        painter->setBrush(QBrush(QColor(144, 238, 144))); // Light green for current sequence
    } else {
        painter->setBrush(QBrush(QColor(200, 200, 200))); // Light gray for normal
    }
    
    painter->setPen(QPen(QColor(100, 100, 100), 2));
    painter->drawRoundedRect(m_rect, 10, 10);
    
    // Draw text
    QFont font;
    font.setPointSize(10);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(QPen(QColor(0, 0, 0), 1));
    
    // Draw name
    QRectF nameRect = QRectF(m_rect.x() + 10, m_rect.y() + 10, m_rect.width() - 20, 20);
    painter->drawText(nameRect, Qt::AlignCenter, m_name);
    
    // Draw description if available
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

// SequenceEdgeItem Implementation
SequenceEdgeItem::SequenceEdgeItem(SequenceNodeItem *source, SequenceNodeItem *dest, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_source(source)
    , m_dest(dest)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    updatePosition();
}

QRectF SequenceEdgeItem::boundingRect() const
{
    qreal extra = 5;
    return QRectF(m_sourcePoint, QSizeF(m_destPoint.x() - m_sourcePoint.x(),
                                       m_destPoint.y() - m_sourcePoint.y()))
        .normalized()
        .adjusted(-extra, -extra, extra, extra);
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

    // Draw arrow
    painter->setPen(QPen(QColor(100, 100, 100), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawLine(line);

    // Draw arrowhead
    double angle = std::atan2(-line.dy(), line.dx());
    QPointF destPoint = m_dest->boundingRect().center();
    
    // Calculate where the arrowhead should connect to the destination
    QLineF normalLine(line);
    qreal length = normalLine.length();
    if (length > 0) {
        qreal ratio = (length - m_dest->boundingRect().width()/3) / length; // Leave some margin
        destPoint = QPointF(line.x1() + ratio * line.dx(), line.y1() + ratio * line.dy());
    }

    // Calculate the angle for the arrowhead
    double angleToDest = std::atan2(destPoint.y() - line.y1(), destPoint.x() - line.x1());
    
    // Calculate arrowhead points
    QPointF arrowP1 = destPoint + QPointF(qSin(angleToDest + M_PI / 3) * 10,
                                          qCos(angleToDest + M_PI / 3) * 10);
    QPointF arrowP2 = destPoint + QPointF(qSin(angleToDest + M_PI - M_PI / 3) * 10,
                                          qCos(angleToDest + M_PI - M_PI / 3) * 10);

    // Draw arrowhead
    painter->setBrush(QColor(100, 100, 100));
    QPolygonF arrowHead;
    arrowHead << destPoint << arrowP1 << arrowP2;
    painter->drawPolygon(arrowHead);
}

void SequenceEdgeItem::updatePosition()
{
    if (!m_source || !m_dest)
        return;

    QLineF line(mapFromItem(m_source, 0, 0), mapFromItem(m_dest, 0, 0));
    qreal length = line.length();

    if (length > qreal(20.)) {
        QPointF edgeOffset((line.dx() * 10) / length, (line.dy() * 10) / length);
        m_sourcePoint = line.p1() + edgeOffset;
        m_destPoint = line.p2() - edgeOffset;
    } else {
        m_sourcePoint = m_destPoint = line.p1();
    }
    
    prepareGeometryChange();
}

// SequenceRelationView Implementation
SequenceRelationView::SequenceRelationView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    
    // Set default view settings
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void SequenceRelationView::setSequenceManager(PromptSequenceManager *seqMgr)
{
    m_seqMgr = seqMgr;
    rebuildGraph();
}

void SequenceRelationView::rebuildGraph()
{
    clearScene();
    
    if (!m_seqMgr) return;
    
    const QMap<QString, PromptSequence>& sequences = m_seqMgr->sequences();
    if (sequences.isEmpty()) return;
    
    // Add all nodes first
    for (auto it = sequences.constBegin(); it != sequences.constEnd(); ++it) {
        const PromptSequence &seq = it.value();
        addNode(seq.name(), seq.description());
    }
    
    // Then find connections between sequences
    findSequenceConnections();
    
    // Position nodes
    positionNodes();
}

void SequenceRelationView::findSequenceConnections()
{
    // This would parse the sequences to find OPEN_SEQUENCE references
    // For now, we'll implement a simple version that looks for connections
    // by examining the templates of each step in each sequence
    
    if (!m_seqMgr) return;
    
    const QMap<QString, PromptSequence>& sequences = m_seqMgr->sequences();
    
    for (auto it = sequences.constBegin(); it != sequences.constEnd(); ++it) {
        const PromptSequence &seq = it.value();
        const QVector<PromptStep>& steps = seq.steps();
        
        // Look for references to other sequences in the templates
        for (const PromptStep &step : steps) {
            QString templateContent = step.tmpl();
            
            // Find all OPEN_SEQUENCE references in the template
            QRegularExpression regex(R"(\{\{OPEN_SEQUENCE:([^,}]+),?([^}]*)\}\})");
            QRegularExpressionMatchIterator iter = regex.globalMatch(templateContent);
            
            while (iter.hasNext()) {
                QRegularExpressionMatch match = iter.next();
                QString targetSequence = match.captured(1).trimmed();
                
                // Check if the target sequence exists
                bool targetExists = false;
                for (auto checkIt = sequences.constBegin(); checkIt != sequences.constEnd(); ++checkIt) {
                    if (checkIt.value().name() == targetSequence) {
                        targetExists = true;
                        break;
                    }
                }
                
                if (targetExists) {
                    addEdge(seq.name(), targetSequence);
                }
            }
        }
    }
}

void SequenceRelationView::addNode(const QString &name, const QString &description)
{
    if (m_nodeItems.contains(name)) return; // Already exists
    
    SequenceNodeItem *item = new SequenceNodeItem(name, description);
    m_scene->addItem(item);
    m_nodeItems[name] = item;
}

void SequenceRelationView::addEdge(const QString &from, const QString &to)
{
    SequenceNodeItem *sourceItem = m_nodeItems.value(from);
    SequenceNodeItem *destItem = m_nodeItems.value(to);
    
    if (!sourceItem || !destItem) return;
    
    SequenceEdgeItem *edgeItem = new SequenceEdgeItem(sourceItem, destItem);
    m_scene->addItem(edgeItem);
    m_edgeItems.append(edgeItem);
}

void SequenceRelationView::positionNodes()
{
    // Simple layout: arrange nodes in a circle
    int count = m_nodeItems.size();
    if (count == 0) return;
    
    int radius = qMax(200, count * 30); // Increase radius based on number of nodes
    QPointF center(sceneRect().width() / 2, sceneRect().height() / 2);
    
    int i = 0;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it, ++i) {
        double angle = 2.0 * M_PI * i / count;
        QPointF pos = center + QPointF(radius * qCos(angle), radius * qSin(angle));
        it.value()->setPos(pos);
    }
    
    // Update all edges
    for (SequenceEdgeItem *edge : m_edgeItems) {
        edge->updatePosition();
    }
    
    // Adjust scene rectangle
    QRectF sceneRect;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        sceneRect = sceneRect.united(it.value()->sceneBoundingRect());
    }
    m_scene->setSceneRect(sceneRect.adjusted(-100, -100, 100, 100));
}

void SequenceRelationView::clearScene()
{
    m_scene->clear();
    m_nodeItems.clear();
    m_edgeItems.clear();
}

void SequenceRelationView::wheelEvent(QWheelEvent *event)
{
    // Zoom in/out with Ctrl + mouse wheel
    if (event->modifiers() & Qt::ControlModifier) {
        double factor = 1.2;
        if (event->angleDelta().y() < 0)
            factor = 1.0 / factor;
        scale(factor, factor);
    } else {
        // Default scrolling behavior
        QGraphicsView::wheelEvent(event);
    }
}

void SequenceRelationView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Check if we double-clicked on a node
    QPointF scenePos = mapToScene(event->pos());
    QGraphicsItem *item = m_scene->itemAt(scenePos, transform());
    
    if (item && item->type() == SequenceNodeItem::Type) {
        SequenceNodeItem *nodeItem = static_cast<SequenceNodeItem*>(item);
        emit sequenceSelected(nodeItem->name());
    }
    
    QGraphicsView::mouseDoubleClickEvent(event);
}

void SequenceRelationView::contextMenuEvent(QContextMenuEvent *event)
{
    showContextMenu(event->pos());
}

void SequenceRelationView::setCurrentSequence(const QString &sequenceName)
{
    // First, reset highlighting for all nodes
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        it.value()->setHighlighted(false);
    }
    
    // Highlight the current sequence node if it exists
    SequenceNodeItem *currentItem = m_nodeItems.value(sequenceName);
    if (currentItem) {
        currentItem->setHighlighted(true);
    }
}

void SequenceRelationView::showContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    
    QAction *refreshAction = menu.addAction("Refresh Graph");
    QAction *selectedAction = menu.exec(mapToGlobal(pos));
    
    if (selectedAction == refreshAction) {
        rebuildGraph();
    }
}