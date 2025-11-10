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
#include <QScrollBar>
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
        QGraphicsScene* currentScene = scene();
        if (currentScene) {
            // Safety check: ensure scene is still valid before using it
            QList<QGraphicsItem*> overlappingItems = currentScene->items(sceneBoundingRect(), Qt::IntersectsItemShape, Qt::AscendingOrder);
            for (QGraphicsItem* item : overlappingItems) {
                if (item && item->type() == SequenceEdgeItem::Type) {
                    SequenceEdgeItem* edge = static_cast<SequenceEdgeItem*>(item);
                    if (edge && (edge->source() == this || edge->dest() == this)) {
                        edge->updatePosition();
                    }
                }
            }
        } else {
            qDebug() << "[SequenceNodeItem] No scene available, skipping edge updates";
            return QGraphicsItem::itemChange(change, value);
        }
        
        // Also save the new position - find the parent SequenceRelationView to call savePositions
        // We can access it through the scene
        if (currentScene) {
            QList<QGraphicsView*> views = currentScene->views();
            if (!views.isEmpty()) {
                // The first view should be the SequenceRelationView
                QGraphicsView* view = views.first();
                if (view) {
                    SequenceRelationView* relationView = qobject_cast<SequenceRelationView*>(view);
                    if (relationView) {
                        // Double-check that the relationView is still valid before calling methods
                        relationView->updateEdgesForNode(this);
                        qDebug() << "[SequenceNodeItem] Calling savePositions for" << m_name;
                        relationView->savePositions();
                        
                        // If we have a current template directory set, save positions to that template as well
                        if (!relationView->currentTemplateDirPath().isEmpty() && relationView->templateManager()) {
                            relationView->savePositionsToTemplate(relationView->currentTemplateDirPath());
                        }
                    }
                } else {
                    qDebug() << "[SequenceNodeItem] View is null, cannot cast";
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
    QPen linePen(QColor(60, 60, 60), 2);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(linePen);
    painter->drawLine(line);

    // Calculate angle of the line for arrowhead (from source to destination)
    double angle = std::atan2(line.dy(), line.dx()); // Direction from source to destination
    
    // Calculate the arrowhead points
    // The arrowhead should point in the direction from source to destination
    QPointF arrowTip = m_destPoint;  // Arrow tip is at the destination
    qreal arrowSize = qMin<qreal>(12.0, line.length() / 4);
    QPointF arrowP1 = arrowTip - QPointF(arrowSize * qCos(angle - M_PI / 6), arrowSize * qSin(angle - M_PI / 6));
    QPointF arrowP2 = arrowTip - QPointF(arrowSize * qCos(angle + M_PI / 6), arrowSize * qSin(angle + M_PI / 6));

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

    // Check if both source and destination are still in the scene
    QGraphicsScene* currentScene = scene();
    if (!currentScene || !currentScene->items().contains(m_source) || !currentScene->items().contains(m_dest)) {
        // If either node is no longer in the scene, skip updating this edge
        return;
    }

    QPointF sourceCenterScene = m_source->sceneBoundingRect().center();
    QPointF destCenterScene = m_dest->sceneBoundingRect().center();

    QPointF sourceScenePoint = getSideCenterPoint(m_source, destCenterScene);
    QPointF destScenePoint = getSideCenterPoint(m_dest, sourceCenterScene);

    m_sourcePoint = mapFromScene(sourceScenePoint);
    m_destPoint = mapFromScene(destScenePoint);

    
    prepareGeometryChange();
}

// Calculate the point at the center of the appropriate side of the node rectangle
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
        if (pointer.intersect(edge, &intersection) == QLineF::BoundedIntersection) {
            return intersection;
        }
    }

    // Fallback to center if intersection wasn't found
    return center;
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
    setDragMode(QGraphicsView::NoDrag);
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
    
    resetTransform();
    m_totalScale = 1.0;

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
        if (edge && m_scene && m_scene->items().contains(edge)) {
            edge->updatePosition();
        }
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
        if (edge && m_scene && m_scene->items().contains(edge)) {
            edge->updatePosition();
        }
    }
    
    qDebug() << "[SequenceRelationView] rebuildGraphForTemplate() completed for:" << templateDirPath;
}

void SequenceRelationView::findSequenceConnectionsForTemplate(const QString &templateDirPath)
{
    // This method finds connections only between sequences that are within the same template
    // It ensures that no external sequence references are shown in the template graph
    
    // Look for template.templ.json in the same directory
    QString mainTemplatePath = QDir(templateDirPath).filePath("template.templ.json");
    if (!QFile::exists(mainTemplatePath)) {
        qDebug() << "[SequenceRelationView] Main template file does not exist in directory:" << templateDirPath;
        // Fallback to old behavior if no template file exists
        findSequenceConnectionsForTemplateLegacy(templateDirPath);
        return;
    }
    
    // Load the main template to get sequence targets
    QFile templateFile(mainTemplatePath);
    if (!templateFile.open(QIODevice::ReadOnly)) {
        qDebug() << "[SequenceRelationView] Could not open template file:" << mainTemplatePath;
        findSequenceConnectionsForTemplateLegacy(templateDirPath);
        return;
    }
    
    QByteArray templateData = templateFile.readAll();
    QJsonDocument templateDoc = QJsonDocument::fromJson(templateData);
    templateFile.close();
    
    if (!templateDoc.isObject()) {
        qDebug() << "[SequenceRelationView] Invalid JSON in template file:" << mainTemplatePath;
        findSequenceConnectionsForTemplateLegacy(templateDirPath);
        return;
    }
    
    QJsonObject templateObj = templateDoc.object();
    
    // Get sequence targets from the template
    if (!templateObj.contains("sequenceTargets")) {
        qDebug() << "[SequenceRelationView] No sequenceTargets in template file, using legacy method";
        findSequenceConnectionsForTemplateLegacy(templateDirPath);
        return;
    }
    
    // Get all available sequence files in the template directory
    QDir dir(templateDirPath);
    if (!dir.exists()) {
        qDebug() << "[SequenceRelationView] Template directory does not exist:" << templateDirPath;
        return;
    }
    
    QStringList filters = {"*.seq.json"};
    QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks);
    
    // Read all sequences in the template to map base names to sequence names
    QMap<QString, QString> baseNameToSequenceNameMap;  // base filename -> sequence name
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
        
        // Map base filename (without extension) to sequence name for lookup
        QString baseName = fileInfo.baseName();
        baseNameToSequenceNameMap[baseName] = seq.name();
        qDebug() << "[SequenceRelationView] Mapped filename" << baseName << "to sequence name" << seq.name();
    }
    
    QJsonObject sequenceTargets = templateObj.value("sequenceTargets").toObject();
    
    // Process each source sequence and its targets from the template
    for (auto it = sequenceTargets.constBegin(); it != sequenceTargets.constEnd(); ++it) {
        QString sourceBaseName = it.key();  // This is the base filename of the source sequence
        QJsonArray targetArray = it.value().toArray();
        
        // Get the actual sequence name for the source
        QString sourceSequenceName = baseNameToSequenceNameMap.value(sourceBaseName, sourceBaseName);
        
        // Process each target sequence
        for (const QJsonValue& targetValue : targetArray) {
            QString targetBaseName = targetValue.toString();
            
            // Get the actual sequence name for the target
            QString targetSequenceName = baseNameToSequenceNameMap.value(targetBaseName, targetBaseName);
            
            // Verify both sequences exist in this template
            if (baseNameToSequenceNameMap.contains(sourceBaseName) && 
                baseNameToSequenceNameMap.contains(targetBaseName)) {
                
                addEdge(sourceSequenceName, targetSequenceName);
                qDebug() << "[SequenceRelationView] Found directed connection:" << sourceSequenceName << "->" << targetSequenceName;
            } else {
                qDebug() << "[SequenceRelationView] Sequence not found for directed connection:" << sourceBaseName << "->" << targetBaseName;
            }
        }
    }
}

void SequenceRelationView::findSequenceConnectionsForTemplateLegacy(const QString &templateDirPath)
{
    // Legacy method to maintain compatibility with older behavior
    // This handles the old OPEN_SEQUENCE:... format (though it's deprecated)
    
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
    QMap<QString, QString> fileToNameMap;         // base filename without extension -> sequence name
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
        
        // Map base filename (without extension) to sequence name for lookup
        QString baseName = fileInfo.baseName();
        fileToNameMap[baseName] = seq.name();
        qDebug() << "[SequenceRelationView] Mapped filename" << baseName << "to sequence name" << seq.name();
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
            // Note: This is legacy support; new approach uses sequenceTargets
            QRegularExpression regex(R"(\{\{OPEN_SEQUENCE:([^,}]+),?([^}]*)\}\})");
            QRegularExpressionMatchIterator iter = regex.globalMatch(templateContent);
            
            while (iter.hasNext()) {
                QRegularExpressionMatch match = iter.next();
                QString targetSequence = match.captured(1).trimmed();
                
                // First check if target sequence exists by its sequence name
                // If not, check if it exists by its base filename (for UI-generated references)
                QString actualTargetSeqName = targetSequence;
                if (!tmplSequences.contains(targetSequence)) {
                    // Try to find using base filename to sequence name mapping
                    if (fileToNameMap.contains(targetSequence)) {
                        actualTargetSeqName = fileToNameMap[targetSequence];
                    }
                }
                
                // CRITICAL: Check if the target sequence exists within the template sequences ONLY
                if (tmplSequences.contains(actualTargetSeqName)) {
                    addEdge(seq.name(), actualTargetSeqName);
                    qDebug() << "[SequenceRelationView] Found internal connection in template:" << seq.name() << "->" << actualTargetSeqName << "(from reference:" << targetSequence << ")";
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
    // New approach: read connections from sequenceTargets in template.templ.json
    // This will show directed links as defined in the template file
    
    if (!m_seqMgr) return;
    
    const QMap<QString, PromptSequence>& sequences = m_seqMgr->sequences();
    
    // Extract template directory from one of the sequence paths
    // We expect sequences to come from the same template directory
    if (sequences.isEmpty()) return;
    
    QString firstSeqPath = sequences.firstKey(); // Get the first sequence file path
    if (firstSeqPath.isEmpty()) return;
    
    QFileInfo seqFileInfo(firstSeqPath);
    QString templateDir = seqFileInfo.absolutePath();
    
    // Look for template.templ.json in the template directory
    QString mainTemplatePath = QDir(templateDir).filePath("template.templ.json");
    if (!QFile::exists(mainTemplatePath)) {
        qDebug() << "[SequenceRelationView] Main template file does not exist:" << mainTemplatePath;
        // Fallback to old behavior if no template file exists
        findSequenceConnectionsLegacy();
        return;
    }
    
    // Load the main template to get sequence targets
    QFile templateFile(mainTemplatePath);
    if (!templateFile.open(QIODevice::ReadOnly)) {
        qDebug() << "[SequenceRelationView] Could not open template file:" << mainTemplatePath;
        findSequenceConnectionsLegacy();
        return;
    }
    
    QByteArray templateData = templateFile.readAll();
    QJsonDocument templateDoc = QJsonDocument::fromJson(templateData);
    templateFile.close();
    
    if (!templateDoc.isObject()) {
        qDebug() << "[SequenceRelationView] Invalid JSON in template file:" << mainTemplatePath;
        findSequenceConnectionsLegacy();
        return;
    }
    
    QJsonObject templateObj = templateDoc.object();
    
    // Get sequence targets from the template
    if (!templateObj.contains("sequenceTargets")) {
        qDebug() << "[SequenceRelationView] No sequenceTargets in template file, using legacy method";
        findSequenceConnectionsLegacy();
        return;
    }
    
    QJsonObject sequenceTargets = templateObj.value("sequenceTargets").toObject();
    
    // Create a mapping from sequence file base name to full sequence information
    QMap<QString, QString> baseNameToSequenceNameMap;  // base filename -> sequence name
    QMap<QString, QString> baseNameToSequencePathMap;  // base filename -> sequence file path
    for (auto it = sequences.constBegin(); it != sequences.constEnd(); ++it) {
        QString filePath = it.key();
        QFileInfo fileInfo(filePath);
        QString baseName = fileInfo.baseName();
        baseNameToSequenceNameMap[baseName] = it.value().name();
        baseNameToSequencePathMap[baseName] = filePath;
    }
    
    // Process each source sequence and its targets
    for (auto it = sequenceTargets.constBegin(); it != sequenceTargets.constEnd(); ++it) {
        QString sourceBaseName = it.key();  // This is the base filename of the source sequence
        QJsonArray targetArray = it.value().toArray();
        
        // Get the actual sequence name for the source
        QString sourceSequenceName = baseNameToSequenceNameMap.value(sourceBaseName, sourceBaseName);
        
        // Process each target sequence
        for (const QJsonValue& targetValue : targetArray) {
            QString targetBaseName = targetValue.toString();
            
            // Get the actual sequence name for the target
            QString targetSequenceName = baseNameToSequenceNameMap.value(targetBaseName, targetBaseName);
            
            // Verify both sequences exist in our manager
            if (baseNameToSequenceNameMap.contains(sourceBaseName) && 
                baseNameToSequenceNameMap.contains(targetBaseName)) {
                
                addEdge(sourceSequenceName, targetSequenceName);
                qDebug() << "[SequenceRelationView] Found directed connection:" << sourceSequenceName << "->" << targetSequenceName;
            } else {
                qDebug() << "[SequenceRelationView] Sequence not found for directed connection:" << sourceBaseName << "->" << targetBaseName;
            }
        }
    }
}

void SequenceRelationView::findSequenceConnectionsLegacy()
{
    // Legacy method to maintain compatibility with older behavior
    // This handles the old OPEN_SEQUENCE:... format (though it's deprecated)
    
    if (!m_seqMgr) return;
    
    const QMap<QString, PromptSequence>& sequences = m_seqMgr->sequences();
    
    // Create a mapping from base filename (without extension) to sequence name
    QMap<QString, QString> fileToNameMap;  // base filename without extension -> sequence name
    for (auto it = sequences.constBegin(); it != sequences.constEnd(); ++it) {
        QString filePath = it.key();  // This is the file path
        QFileInfo fileInfo(filePath);
        QString baseName = fileInfo.baseName();  // Get the filename without extension
        fileToNameMap[baseName] = it.value().name();  // Map base name to sequence name
        qDebug() << "[SequenceRelationView] Mapped filename" << baseName << "to sequence name" << it.value().name();
    }
    
    for (auto it = sequences.constBegin(); it != sequences.constEnd(); ++it) {
        const PromptSequence &seq = it.value();
        const QVector<PromptStep>& steps = seq.steps();
        
        // Look for references to other sequences in the templates
        for (const PromptStep &step : steps) {
            QString templateContent = step.tmpl();
            
            // Find all OPEN_SEQUENCE references in the template
            // Note: This is legacy support; new approach uses sequenceTargets
            QRegularExpression regex(R"(\{\{OPEN_SEQUENCE:([^,}]+),?([^}]*)\}\})");
            QRegularExpressionMatchIterator iter = regex.globalMatch(templateContent);
            
            while (iter.hasNext()) {
                QRegularExpressionMatch match = iter.next();
                QString targetSequence = match.captured(1).trimmed();
                
                // First check if target sequence exists by its sequence name
                // If not, check if it exists by its base filename (for UI-generated references)
                QString actualTargetSeqName = targetSequence;
                bool targetSeqExistsByName = false;
                for (auto checkIt = sequences.constBegin(); checkIt != sequences.constEnd(); ++checkIt) {
                    if (checkIt.value().name() == targetSequence) {
                        targetSeqExistsByName = true;
                        break;
                    }
                }
                
                if (!targetSeqExistsByName) {
                    // Try to find using base filename to sequence name mapping
                    if (fileToNameMap.contains(targetSequence)) {
                        actualTargetSeqName = fileToNameMap[targetSequence];
                    }
                }
                
                // Check if the actual target sequence exists
                bool targetExists = false;
                for (auto checkIt = sequences.constBegin(); checkIt != sequences.constEnd(); ++checkIt) {
                    if (checkIt.value().name() == actualTargetSeqName) {
                        targetExists = true;
                        break;
                    }
                }
                
                if (targetExists) {
                    addEdge(seq.name(), actualTargetSeqName);
                    qDebug() << "[SequenceRelationView] Found legacy connection:" << seq.name() << "->" << actualTargetSeqName << "(from reference:" << targetSequence << ")";
                } else {
                    qDebug() << "[SequenceRelationView] Target sequence not found:" << targetSequence << "referenced from:" << seq.name();
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
    m_nodeEdges[sourceItem].append(edgeItem);
    m_nodeEdges[destItem].append(edgeItem);
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
        if (edge && m_scene && m_scene->items().contains(edge)) {
            edge->updatePosition();
        }
    }
    
    // Adjust scene rectangle
    QRectF sceneRect;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        sceneRect = sceneRect.united(it.value()->sceneBoundingRect());
    }
    m_scene->setSceneRect(sceneRect.adjusted(-100, -100, 100, 100));
}

void SequenceRelationView::updateEdgesForNode(SequenceNodeItem *node)
{
    if (!node || !m_nodeEdges.contains(node))
        return;

    const QList<SequenceEdgeItem*> edges = m_nodeEdges.value(node);
    for (SequenceEdgeItem *edge : edges) {
        if (edge && m_scene && m_scene->items().contains(edge)) {
            edge->updatePosition();
        }
    }
}

void SequenceRelationView::fitToContents()
{
    if (m_nodeItems.isEmpty())
        return;

    QRectF bounds;
    for (SequenceNodeItem *item : qAsConst(m_nodeItems)) {
        bounds = bounds.united(item->sceneBoundingRect());
    }

    if (bounds.isNull())
        return;

    bounds = bounds.adjusted(-80, -80, 80, 80);
    fitInView(bounds, Qt::KeepAspectRatio);
    m_totalScale = transform().m11();
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
    if (event->modifiers() & Qt::ControlModifier) {
        constexpr qreal ScaleStep = 1.15;
        qreal delta = event->angleDelta().y() / 120.0;
        qreal factor = qPow(ScaleStep, delta);
        qreal targetScale = qBound(0.25, m_totalScale * factor, 6.0);
        qreal actualFactor = targetScale / m_totalScale;
        scale(actualFactor, actualFactor);
        m_totalScale = targetScale;
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void SequenceRelationView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && event->modifiers() & Qt::AltModifier)) {
        m_panning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void SequenceRelationView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        QPoint delta = event->pos() - m_lastPanPoint;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_lastPanPoint = event->pos();
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void SequenceRelationView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && (event->button() == Qt::MiddleButton ||
                      (event->button() == Qt::LeftButton && event->modifiers() & Qt::AltModifier))) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
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
        SequenceNodeItem* node = it.value();
        
        // Check if node still exists in scene before getting its position
        if (node && m_scene && m_scene->items().contains(node)) {
            QPointF pos = node->pos();

            // Update the internal map with current position
            m_nodePositions[nodeName] = pos;
            
            qDebug() << "[SequenceRelationView] Saved position for node:" << nodeName << "at" << pos;
        } else {
            qDebug() << "[SequenceRelationView] Node" << nodeName << "not found in scene, skipping";
        }
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


