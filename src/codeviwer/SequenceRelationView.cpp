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
#include <QSettings>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "SolutionTemplate.h"
#include "SolutionTemplateManager.h"

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

QVariant SequenceNodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        qDebug() << "[SequenceNodeItem]" << m_name << "position changed to:" << value.toPointF();
        
        // When this node moves, we need to update all edges connected to it
        // Get only the edges that overlap with this item's movement to be more efficient
        QList<QGraphicsItem*> overlappingItems = scene()->items(sceneBoundingRect(), Qt::IntersectsItemShape, Qt::AscendingOrder);
        for (QGraphicsItem* item : overlappingItems) {
            if (item->type() == SequenceEdgeItem::Type) {
                SequenceEdgeItem* edge = static_cast<SequenceEdgeItem*>(item);
                if (edge->source() == this || edge->dest() == this) {
                    edge->updatePosition();
                }
            }
        }
        
        // Also save the new position - find the parent SequenceRelationView to call savePositions
        // We can access it through the scene
        QGraphicsScene* currentScene = scene();
        if (currentScene) {
            QList<QGraphicsView*> views = currentScene->views();
            if (!views.isEmpty()) {
                // The first view should be the SequenceRelationView
                SequenceRelationView* relationView = qobject_cast<SequenceRelationView*>(views.first());
                if (relationView) {
                    qDebug() << "[SequenceNodeItem] Calling savePositions for" << m_name;
                    relationView->savePositions();
                    
                    // If we have a current template directory set, save positions to that template as well
                    if (!relationView->currentTemplateDirPath().isEmpty() && relationView->templateManager()) {
                        relationView->savePositionsToTemplate(relationView->currentTemplateDirPath());
                    }
                } else {
                    qDebug() << "[SequenceNodeItem] Could not cast view to SequenceRelationView";
                }
            } else {
                qDebug() << "[SequenceNodeItem] No views found for scene";
            }
        } else {
            qDebug() << "[SequenceNodeItem] No scene available";
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

// SequenceEdgeItem Implementation
SequenceEdgeItem::SequenceEdgeItem(SequenceNodeItem *source, SequenceNodeItem *dest, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_source(source)
    , m_dest(dest)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    updatePosition();
}

QRectF SequenceEdgeItem::boundingRect() const
{
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

    // Draw arrow line
    painter->setPen(QPen(QColor(100, 100, 100), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawLine(line);

    // Calculate angle of the line for arrowhead (from source to destination)
    double angle = std::atan2(line.dy(), line.dx()); // Direction from source to destination
    
    // Calculate the arrowhead points
    // The arrowhead should point in the direction from source to destination
    QPointF arrowTip = m_destPoint;  // Arrow tip is at the destination
    QPointF arrowP1 = arrowTip - QPointF(10 * qCos(angle - M_PI / 6), 10 * qSin(angle - M_PI / 6));
    QPointF arrowP2 = arrowTip - QPointF(10 * qCos(angle + M_PI / 6), 10 * qSin(angle + M_PI / 6));

    // Draw arrowhead
    painter->setBrush(QColor(100, 100, 100));
    QPolygonF arrowHead;
    arrowHead << arrowTip << arrowP1 << arrowP2;
    painter->drawPolygon(arrowHead);
}

void SequenceEdgeItem::updatePosition()
{
    if (!m_source || !m_dest)
        return;

    // Map the center points of source and destination nodes to this item's coordinate system
    QPointF sourceCenter = mapFromItem(m_source, 0, 0);  
    QPointF destCenter = mapFromItem(m_dest, 0, 0);      

    // Calculate positions for connection points at the center of the appropriate side
    m_sourcePoint = getSideCenterPoint(m_source, destCenter);
    m_destPoint = getSideCenterPoint(m_dest, sourceCenter);
    
    prepareGeometryChange();
}

// Calculate the point at the center of the appropriate side of the node rectangle
QPointF SequenceEdgeItem::getSideCenterPoint(SequenceNodeItem *node, const QPointF &targetPoint) const
{
    if (!node) return QPointF();
    
    // Get the node's bounding rectangle and position in the edge's coordinate system
    QRectF nodeRect = node->boundingRect();  // Local bounding rectangle of the node
    QPointF nodeCenterInEdgeCoords = mapFromItem(node, 0, 0);  // Center of node in edge coordinates
    
    // Calculate the actual rectangle in the edge's coordinate system
    QRectF rectInEdgeCoords(
        nodeCenterInEdgeCoords.x() + nodeRect.left(),
        nodeCenterInEdgeCoords.y() + nodeRect.top(),
        nodeRect.width(),
        nodeRect.height()
    );
    
    // Calculate direction vector from node center to target
    qreal dx = targetPoint.x() - nodeCenterInEdgeCoords.x();
    qreal dy = targetPoint.y() - nodeCenterInEdgeCoords.y();
    
    // Determine which side to connect to based on the direction
    qreal absDx = qAbs(dx);
    qreal absDy = qAbs(dy);
    qreal aspectRatio = rectInEdgeCoords.width() / rectInEdgeCoords.height();
    qreal slopeRatio = (absDx > 0) ? (absDy / absDx) : 1000000.0; // Avoid division by zero
    
    QPointF connectionPoint;
    
    if (slopeRatio <= 1.0 / aspectRatio) {
        // Connect to left or right side (slope is relatively horizontal)
        if (dx > 0) {
            // Connect to right side center (middle of right edge)
            connectionPoint = QPointF(rectInEdgeCoords.right(), rectInEdgeCoords.center().y());
        } else {
            // Connect to left side center (middle of left edge)  
            connectionPoint = QPointF(rectInEdgeCoords.left(), rectInEdgeCoords.center().y());
        }
    } else {
        // Connect to top or bottom side (slope is relatively vertical)
        if (dy > 0) {
            // Connect to bottom side center (middle of bottom edge)
            connectionPoint = QPointF(rectInEdgeCoords.center().x(), rectInEdgeCoords.bottom());
        } else {
            // Connect to top side center (middle of top edge)
            connectionPoint = QPointF(rectInEdgeCoords.center().x(), rectInEdgeCoords.top());
        }
    }
    
    return connectionPoint;
}

QVariant SequenceEdgeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    // The edge itself doesn't move independently - it only updates based on node positions
    return QGraphicsItem::itemChange(change, value);
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
    qDebug() << "[SequenceRelationView] setSequenceManager called";
    m_seqMgr = seqMgr;
    // Don't rebuild immediately - let the caller decide when to rebuild
    // This allows restoreLayout to happen before the graph is built
    // rebuildGraph();  // Remove automatic rebuild
}

void SequenceRelationView::setTemplateManager(SolutionTemplateManager *tmplMgr)
{
    qDebug() << "[SequenceRelationView] setTemplateManager called";
    m_templateMgr = tmplMgr;
}

void SequenceRelationView::rebuildGraph()
{
    qDebug() << "[SequenceRelationView] rebuildGraph() called";
    
    // Store current positions before clearing the scene
    QMap<QString, QPointF> savedPositions = m_nodePositions;
    
    qDebug() << "[SequenceRelationView] Saved positions before clearing:" << savedPositions;
    
    clearScenePreservingPositions();
    
    if (!m_seqMgr) {
        qDebug() << "[SequenceRelationView] No sequence manager, returning";
        return;
    }
    
    const QMap<QString, PromptSequence>& sequences = m_seqMgr->sequences();
    if (sequences.isEmpty()) {
        qDebug() << "[SequenceRelationView] No sequences, returning";
        return;
    }
    
    qDebug() << "[SequenceRelationView] Adding nodes for sequences:" << sequences.keys();
    
    // Add all nodes first
    for (auto it = sequences.constBegin(); it != sequences.constEnd(); ++it) {
        const PromptSequence &seq = it.value();
        addNode(seq.name(), seq.description());
    }
    
    // Then find connections between sequences
    findSequenceConnections();
    
    qDebug() << "[SequenceRelationView] Restoring saved positions after adding nodes";
    
    // For non-template graphs, we only use the saved positions from the current session
    // since there's no template file to load positions from
    
    // Apply any saved positions from the internal map (e.g. from current session)
    for (auto it = savedPositions.constBegin(); it != savedPositions.constEnd(); ++it) {
        QString nodeName = it.key();
        QPointF pos = it.value();
        if (m_nodeItems.contains(nodeName)) {
            m_nodeItems[nodeName]->setPos(pos);
            m_nodePositions[nodeName] = pos;  // Update our internal map
            qDebug() << "[SequenceRelationView] Restored position (from internal) for node:" << nodeName << "at" << pos;
        }
    }
    
    // If still no positions found after both sources, use default layout
    if (m_nodePositions.isEmpty()) {
        qDebug() << "[SequenceRelationView] No saved positions found, using default layout";
        positionNodes();
    } else {
        qDebug() << "[SequenceRelationView] Saved positions restored, nodes in scene:" << m_nodeItems.size();
    }
    
    // Update all edges to reflect current positions
    for (SequenceEdgeItem *edge : m_edgeItems) {
        edge->updatePosition();
    }
    
    qDebug() << "[SequenceRelationView] rebuildGraph() completed";
}

void SequenceRelationView::rebuildGraphForTemplate(const QString &templateDirPath)
{
    qDebug() << "[SequenceRelationView] rebuildGraphForTemplate called for:" << templateDirPath;
    
    // Store current positions before clearing the scene
    QMap<QString, QPointF> savedPositions = m_nodePositions;
    
    qDebug() << "[SequenceRelationView] Saved positions before clearing:" << savedPositions;
    
    clearScenePreservingPositions();
    
    // Set the current template directory so positions can be saved to this template
    setCurrentTemplateDir(templateDirPath);
    
    // Get all available sequence files in the template directory
    QDir dir(templateDirPath);
    if (!dir.exists()) {
        qDebug() << "[SequenceRelationView] Template directory does not exist:" << templateDirPath;
        return;
    }
    
    QStringList filters = {"*.seq.json"};
    QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks);
    
    // Load and add only sequences that exist in this template
    for (const QFileInfo& fileInfo : fileInfoList) {
        QString seqPath = fileInfo.absoluteFilePath();
        
        QFile seqFile(seqPath);
        if (!seqFile.open(QIODevice::ReadOnly)) {
            qDebug() << "[SequenceRelationView] Could not open sequence file:" << seqPath;
            continue;
        }
        
        QByteArray seqData = seqFile.readAll();
        QJsonDocument seqDoc = QJsonDocument::fromJson(seqData);
        if (!seqDoc.isObject()) {
            qDebug() << "[SequenceRelationView] Invalid JSON in sequence file:" << seqPath;
            continue;
        }
        
        PromptSequence seq = PromptSequence::fromJson(seqDoc.object());
        addNode(seq.name(), seq.description());
    }
    
    // Find connections within this template
    findSequenceConnectionsForTemplate(templateDirPath);
    
    qDebug() << "[SequenceRelationView] Restoring saved positions after adding nodes for template";
    
    // Load positions from the template file instead of settings
    if (m_templateMgr) {
        QMap<QString, QPointF> templatePositions = m_templateMgr->loadPositionsFromTemplate(templateDirPath);
        for (auto it = templatePositions.constBegin(); it != templatePositions.constEnd(); ++it) {
            QString nodeName = it.key();
            QPointF pos = it.value();
            if (m_nodeItems.contains(nodeName)) {
                m_nodeItems[nodeName]->setPos(pos);
                m_nodePositions[nodeName] = pos;  // Update our internal map
                qDebug() << "[SequenceRelationView] Loaded position from template for node:" << nodeName << "at" << pos;
            }
        }
    }
    
    // If there were any more recent positions in the internal map (e.g. from the current session before the last save),
    // these might be more current than what was in settings prior to the current session
    // However, we should only apply them if they differ from what was loaded from template
    for (auto it = savedPositions.constBegin(); it != savedPositions.constEnd(); ++it) {
        QString nodeName = it.key();
        QPointF pos = it.value();
        if (m_nodeItems.contains(nodeName)) {
            // Only update if the saved position from internal map differs from what's in m_nodePositions after loading from template
            if (!m_nodePositions.contains(nodeName) || m_nodePositions[nodeName] != pos) {
                m_nodeItems[nodeName]->setPos(pos);
                m_nodePositions[nodeName] = pos;  // Update our internal map
                qDebug() << "[SequenceRelationView] Restored position (from internal, possibly more recent) for node:" << nodeName << "at" << pos;
            } else {
                qDebug() << "[SequenceRelationView] Skipping internal position for node:" << nodeName << ", already loaded from template";
            }
        }
    }
    
    // If still no positions found after both sources, use default layout
    if (m_nodePositions.isEmpty()) {
        qDebug() << "[SequenceRelationView] No saved positions found, using default layout";
        positionNodes();
    } else {
        qDebug() << "[SequenceRelationView] Saved positions restored, nodes in scene:" << m_nodeItems.size();
    }
    
    // Update all edges to reflect current positions
    for (SequenceEdgeItem *edge : m_edgeItems) {
        edge->updatePosition();
    }
    
    qDebug() << "[SequenceRelationView] rebuildGraphForTemplate() completed for:" << templateDirPath;
}

void SequenceRelationView::findSequenceConnectionsForTemplate(const QString &templateDirPath)
{
    // This method finds connections only between sequences that are within the same template
    // It ensures that no external sequence references are shown in the template graph
    
    // Get all available sequence files in the template directory
    QDir dir(templateDirPath);
    if (!dir.exists()) {
        qDebug() << "[SequenceRelationView] Template directory does not exist:" << templateDirPath;
        return;
    }
    
    QStringList filters = {"*.seq.json"};
    QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks);
    
    // First, read all sequences in the template to load them into memory
    QMap<QString, PromptSequence> tmplSequences;  // name -> sequence
    for (const QFileInfo& fileInfo : fileInfoList) {
        QString seqPath = fileInfo.absoluteFilePath();
        
        QFile seqFile(seqPath);
        if (!seqFile.open(QIODevice::ReadOnly)) {
            qDebug() << "[SequenceRelationView] Could not open sequence file:" << seqPath;
            continue;
        }
        
        QByteArray seqData = seqFile.readAll();
        QJsonDocument seqDoc = QJsonDocument::fromJson(seqData);
        if (!seqDoc.isObject()) {
            qDebug() << "[SequenceRelationView] Invalid JSON in sequence file:" << seqPath;
            continue;
        }
        
        PromptSequence seq = PromptSequence::fromJson(seqDoc.object());
        tmplSequences[seq.name()] = seq;
    }
    
    // Now find connections between sequences in the template
    // ONLY between sequences that are part of this template
    for (auto it = tmplSequences.constBegin(); it != tmplSequences.constEnd(); ++it) {
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
                
                // CRITICAL: Check if the target sequence exists within the template sequences ONLY
                if (tmplSequences.contains(targetSequence)) {
                    addEdge(seq.name(), targetSequence);
                    qDebug() << "[SequenceRelationView] Found internal connection in template:" << seq.name() << "->" << targetSequence;
                } else {
                    // This is an external reference that should not be shown in the template graph
                    qDebug() << "[SequenceRelationView] External reference found but ignored in template graph:" << seq.name() << "->" << targetSequence;
                }
            }
        }
    }
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
    qDebug() << "[SequenceRelationView] addNode called for:" << name << "with current m_nodePositions size:" << m_nodePositions.size();
    
    if (m_nodeItems.contains(name)) {
        qDebug() << "[SequenceRelationView] Node" << name << "already exists, skipping";
        return; // Already exists
    }
    
    SequenceNodeItem *item = new SequenceNodeItem(name, description);
    m_scene->addItem(item);
    m_nodeItems[name] = item;
    
    // Initialize position in our tracking map if it exists in saved positions
    if (m_nodePositions.contains(name)) {
        QPointF pos = m_nodePositions[name];
        item->setPos(pos);
        qDebug() << "[SequenceRelationView] Set position for node:" << name << "at" << pos;
    } else {
        qDebug() << "[SequenceRelationView] No saved position for node:" << name << ", keeping default position";
    }
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
    
    // Force an update for all edges to recalculate their positions after nodes are positioned
    // Batch update to improve performance
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
    m_nodePositions.clear();  // Clear positions map when scene is cleared completely
}

void SequenceRelationView::clearScenePreservingPositions()
{
    m_scene->clear();
    m_nodeItems.clear();
    m_edgeItems.clear();
    // Don't clear m_nodePositions to preserve positions for rebuild
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

void SequenceRelationView::saveNodePositions()
{
    qDebug() << "[SequenceRelationView] Saving node positions to internal map, number of nodes:" << m_nodeItems.size();
    
    // Clear the internal positions map and rebuild it from current node positions
    m_nodePositions.clear();
    
    // Update internal map with current positions
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        QString nodeName = it.key();
        QPointF pos = it.value()->pos();
        
        // Update the internal map with current position
        m_nodePositions[nodeName] = pos;
        
        qDebug() << "[SequenceRelationView] Saved position for node:" << nodeName << "at" << pos;
    }
}

// New method to save positions to a specific template directory
void SequenceRelationView::savePositionsToTemplate(const QString& templateDirPath)
{
    qDebug() << "[SequenceRelationView] Saving positions to template directory:" << templateDirPath;
    
    // Update internal positions map
    saveNodePositions(); // This will update m_nodePositions with current positions
    
    // If we have a template manager, save positions to the template file
    if (m_templateMgr) {
        m_templateMgr->savePositionsToTemplate(templateDirPath, m_nodePositions);
    } else {
        qDebug() << "[SequenceRelationView] No template manager available to save positions";
    }
}

void SequenceRelationView::savePositions()
{
    // Call the private method to save current positions
    saveNodePositions();
    
    // If we have a current template directory set, save positions to that template as well
    if (!m_currentTemplateDirPath.isEmpty() && m_templateMgr) {
        savePositionsToTemplate(m_currentTemplateDirPath);
    }
}

void SequenceRelationView::saveLayout()
{
    // If we have a template directory set, save positions to the template
    if (!m_currentTemplateDirPath.isEmpty() && m_templateMgr) {
        savePositionsToTemplate(m_currentTemplateDirPath);
    } else {
        // Otherwise, just save to internal positions map
        saveNodePositions();
    }
}

void SequenceRelationView::restoreLayout()
{
    qDebug() << "[SequenceRelationView] restoreLayout() called";
    // For backward compatibility, we just load from internal map
    // When building a template graph, positions will be loaded from the template file
    qDebug() << "[SequenceRelationView] restoreLayout() completed, positions in m_nodePositions:" << m_nodePositions.size();
}

// Method to set the current template directory path
void SequenceRelationView::setCurrentTemplateDir(const QString& templateDirPath)
{
    m_currentTemplateDirPath = templateDirPath;
    qDebug() << "[SequenceRelationView] Set current template directory to:" << templateDirPath;
}

void SequenceRelationView::loadNodePositions()
{
    qDebug() << "[SequenceRelationView] Loading node positions";
    
    // For now, we'll clear the position map and let individual templates load their position data
    // when they are loaded via SolutionTemplateManager
    
    // If we have positions already stored in our internal map, we'll use them
    // In the future, this will come from template files that are loaded by SolutionTemplateManager
}