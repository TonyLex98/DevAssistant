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
#include <QKeyEvent>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QGraphicsSceneHoverEvent>
#include <QLinearGradient>
#include <QMessageBox>
#include <QDir>
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
    setAcceptHoverEvents(true);
    if (!m_description.isEmpty()) {
        setToolTip(QString("%1\n%2").arg(m_name, m_description));
    } else {
        setToolTip(m_name);
    }
    
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
    QColor fillColor(200, 200, 200);
    if (m_highlighted) {
        fillColor = QColor(144, 238, 144); // Light green for current sequence
    } else if (m_connectionSource) {
        fillColor = QColor(255, 230, 180);
    } else if (isSelected()) {
        fillColor = QColor(180, 205, 255);
    } else if (m_hovered || m_connectionTargetHover) {
        fillColor = QColor(215, 215, 230); // Slightly brighter on hover/target
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

void SequenceNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    // Highlight as potential target when connecting
    if (scene()) {
        auto views = scene()->views();
        if (!views.isEmpty()) {
            SequenceRelationView *view = qobject_cast<SequenceRelationView*>(views.first());
            if (view && view->isConnecting() && view->connectionSource() != this) {
                m_connectionTargetHover = true;
            }
        }
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

// SequenceEdgeItem Implementation
SequenceEdgeItem::SequenceEdgeItem(SequenceNodeItem *source, SequenceNodeItem *dest, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_source(source)
    , m_dest(dest)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setZValue(-1); // Устанавливаем z-значение, чтобы стрелки отображались под узлами
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
    setRenderHint(QPainter::TextAntialiasing);
    setCacheMode(QGraphicsView::CacheBackground);
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    setOptimizationFlag(QGraphicsView::IndirectPainting, true);
    setBackgroundBrush(QColor(245, 245, 250));
    
    // Set default view settings
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
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
    
    QTransform prevTransform = transform();
    QPointF prevCenter = mapToScene(viewport()->rect().center());
    bool hadViewState = !m_scene->items().isEmpty();

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
    
    m_sequenceToBaseName.clear();
    // Add all nodes first
    for (auto it = sequences.constBegin(); it != sequences.constEnd(); ++it) {
        const PromptSequence &seq = it.value();
        QString baseName = QFileInfo(it.key()).baseName();
        m_sequenceToBaseName.insert(seq.name(), baseName);
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
    
    bool restored = false;
    if (hadViewState && !m_scene->items().isEmpty()) {
        setTransform(prevTransform);
        centerOn(prevCenter);
        m_totalScale = transform().m11();
        restored = true;
    }

    if (!restored) {
        fitToContents();
    }
    
    qDebug() << "[SequenceRelationView] rebuildGraph() completed";
}

void SequenceRelationView::rebuildGraphForTemplate(const QString &templateDirPath)
{
    qDebug() << "[SequenceRelationView] rebuildGraphForTemplate called for:" << templateDirPath;
    QTransform prevTransform = transform();
    QPointF prevCenter = mapToScene(viewport()->rect().center());
    bool hadViewState = !m_scene->items().isEmpty();
    
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
    
    m_sequenceToBaseName.clear();
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
        m_sequenceToBaseName.insert(seq.name(), fileInfo.baseName());
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
    
    bool restored = false;
    if (hadViewState && !m_scene->items().isEmpty()) {
        setTransform(prevTransform);
        centerOn(prevCenter);
        m_totalScale = transform().m11();
        restored = true;
    }

    if (!restored) {
        fitToContents();
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
    setEdgesFromTemplate(sequenceTargets, baseNameToSequenceNameMap);
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

    setEdgesFromTemplate(sequenceTargets, baseNameToSequenceNameMap);
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
    QString key = from + "->" + to;
    if (m_edgesSet.contains(key))
        return;

    SequenceNodeItem *sourceItem = m_nodeItems.value(from);
    SequenceNodeItem *destItem = m_nodeItems.value(to);
    
    if (!sourceItem || !destItem) return;
    
    SequenceEdgeItem *edgeItem = new SequenceEdgeItem(sourceItem, destItem);
    m_scene->addItem(edgeItem);
    m_edgeItems.append(edgeItem);
    m_nodeEdges[sourceItem].append(edgeItem);
    m_nodeEdges[destItem].append(edgeItem);
    m_edgesSet.insert(key);
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
    
    // Adjust scene rectangle with more flexible bounds for infinite workspace
    QRectF sceneRect;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        sceneRect = sceneRect.united(it.value()->sceneBoundingRect());
    }
    if (sceneRect.isEmpty()) {
        // Default scene size if no items
        m_scene->setSceneRect(-500, -500, 1000, 1000);
    } else {
        // Use the bounding rect but ensure minimum size for panning
        QRectF adjustedRect = sceneRect.adjusted(-100, -100, 100, 100);
        // Ensure minimum size for panning
        qreal width = qMax(adjustedRect.width(), 1000.0);
        qreal height = qMax(adjustedRect.height(), 1000.0);
        qreal left = adjustedRect.left() - (width - adjustedRect.width()) / 2;
        qreal top = adjustedRect.top() - (height - adjustedRect.height()) / 2;
        m_scene->setSceneRect(left, top, width, height);
    }
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

void SequenceRelationView::removeEdgeItem(const QString &sourceName, const QString &targetName)
{
    QString key = sourceName + "->" + targetName;
    for (int i = 0; i < m_edgeItems.size(); ++i) {
        SequenceEdgeItem *edge = m_edgeItems.at(i);
        if (edge && edge->source() && edge->dest() &&
            edge->source()->name() == sourceName && edge->dest()->name() == targetName) {
            m_scene->removeItem(edge);
            m_nodeEdges[edge->source()].removeAll(edge);
            m_nodeEdges[edge->dest()].removeAll(edge);
            delete edge;
            m_edgeItems.removeAt(i);
            m_edgesSet.remove(key);
            break;
        }
    }
}

void SequenceRelationView::removeNodeItem(const QString &sequenceName)
{
    SequenceNodeItem *item = m_nodeItems.take(sequenceName);
    if (!item)
        return;

    // Remove connected edges
    QList<SequenceEdgeItem*> edges = m_nodeEdges.take(item);
    for (SequenceEdgeItem *edge : edges) {
        if (!edge) continue;
        QString key = (edge->source() ? edge->source()->name() : QString()) + "->" +
                      (edge->dest() ? edge->dest()->name() : QString());
        m_edgesSet.remove(key);
        SequenceNodeItem *other = edge->source() == item ? edge->dest() : edge->source();
        if (other && m_nodeEdges.contains(other)) {
            m_nodeEdges[other].removeAll(edge);
        }
        m_scene->removeItem(edge);
        delete edge;
        m_edgeItems.removeOne(edge);
    }

    m_scene->removeItem(item);
    delete item;
    m_nodePositions.remove(sequenceName);
}

void SequenceRelationView::fitToContents()
{
    QRectF bounds = contentBounds();
    if (bounds.isNull() || !bounds.isValid())
        return;

    bounds = bounds.adjusted(-80, -80, 80, 80);
    fitInView(bounds, Qt::KeepAspectRatio);
    m_totalScale = transform().m11();
}

void SequenceRelationView::clearScene()
{
    cancelConnection();
    m_scene->clear();
    m_nodeItems.clear();
    m_edgeItems.clear();
    m_nodePositions.clear();  // Clear positions map when scene is cleared completely
    m_edgesSet.clear();
    m_sequenceToBaseName.clear();
    cancelConnection();
}

void SequenceRelationView::clearScenePreservingPositions()
{
    cancelConnection();
    m_scene->clear();
    m_nodeItems.clear();
    m_edgeItems.clear();
    // Don't clear m_nodePositions to preserve positions for rebuild
    m_edgesSet.clear();
    m_sequenceToBaseName.clear();
    cancelConnection();
}

void SequenceRelationView::wheelEvent(QWheelEvent *event)
{
    const bool zooming = event->modifiers().testFlag(Qt::ControlModifier);
    if (!zooming) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    qreal delta = event->angleDelta().y();
    if (qFuzzyIsNull(delta) && !event->pixelDelta().isNull()) {
        delta = event->pixelDelta().y();
    }

    if (qFuzzyIsNull(delta)) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    const QPoint viewportPos = event->pos();
    const QPointF scenePosBefore = mapToScene(viewportPos);

    qreal steps = delta / 120.0; // 120 == one mouse wheel step
    qreal factor = qPow(m_zoomStep, steps);
    qreal targetScale = qBound(m_minScale, m_totalScale * factor, m_maxScale);
    qreal actualFactor = targetScale / m_totalScale;

    if (qFuzzyCompare(actualFactor, 1.0)) {
        event->accept();
        return;
    }

    scale(actualFactor, actualFactor);
    m_totalScale = targetScale;

    // Keep the point under cursor stationary in view coordinates
    const QPointF scenePosAfter = mapToScene(viewportPos);
    const QPointF move = scenePosAfter - scenePosBefore;
    translate(-move.x(), -move.y());
    event->accept();
}

void SequenceRelationView::mousePressEvent(QMouseEvent *event)
{
    const bool panButton = event->button() == Qt::MiddleButton ||
                           (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier)) ||
                           (event->button() == Qt::LeftButton && m_spacePanning);
    if (panButton) {
        m_panning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (m_connectSource && event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        SequenceNodeItem *targetNode = nullptr;
        const QList<QGraphicsItem*> itemsAtPos = m_scene->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder, transform());
        for (QGraphicsItem *it : itemsAtPos) {
            if (it && it->type() == SequenceNodeItem::Type) {
                SequenceNodeItem *candidate = static_cast<SequenceNodeItem*>(it);
                if (candidate != m_connectSource) {
                    targetNode = candidate;
                    break;
                }
            }
        }
        if (targetNode) {
            if (!addDirectedConnection(m_connectSource->name(), targetNode->name())) {
                QMessageBox::warning(this, tr("Связь не создана"),
                                     tr("Не удалось создать связь между %1 и %2.")
                                         .arg(m_connectSource->name(), targetNode->name()));
            }
            cancelConnection();
        } else {
            // keep connection mode active, just update temp line
            updateTempConnectionTo(scenePos);
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());

        // Если нажата клавиша Shift, выбираем нижний элемент (в порядке z-значений)
        Qt::SortOrder order = event->modifiers() & Qt::ShiftModifier ? Qt::AscendingOrder : Qt::DescendingOrder;
        QList<QGraphicsItem*> itemsAtPos = m_scene->items(scenePos, Qt::IntersectsItemShape, order, transform());

        // Ищем SequenceNodeItem среди элементов
        SequenceNodeItem *targetNode = nullptr;
        for (QGraphicsItem *item : itemsAtPos) {
            if (item && item->type() == SequenceNodeItem::Type) {
                targetNode = static_cast<SequenceNodeItem*>(item);
                break; // Выбираем первый элемент в зависимости от порядка сортировки
            }
        }

        if (targetNode) {
            setCurrentSequence(targetNode->name());
        } else {
            // If no node was clicked, start panning
            m_panning = true;
            m_lastPanPoint = event->pos();
            setCursor(Qt::ClosedHandCursor); // Change cursor to indicate panning
            event->accept();
            return;
        }
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

        // Expand scene if we're panning near the edges
        expandSceneIfNeeded();

        event->accept();
        return;
    }

    if (m_connectSource) {
        updateTempConnectionTo(mapToScene(event->pos()));
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void SequenceRelationView::expandSceneIfNeeded() {
    // Get current view area in scene coordinates
    QRectF viewRect = mapToScene(rect()).boundingRect();

    // Get current scene rectangle
    QRectF sceneRect = scene()->sceneRect();

    // Calculate margins - expand if view is near the edge of the scene
    qreal margin = 200.0; // pixels of margin

    qreal newLeft = sceneRect.left();
    qreal newTop = sceneRect.top();
    qreal newRight = sceneRect.right();
    qreal newBottom = sceneRect.bottom();

    // Check if we need to expand in any direction
    bool needsExpansion = false;

    if (viewRect.left() < sceneRect.left() + margin) {
        newLeft = viewRect.left() - margin * 2;
        needsExpansion = true;
    }
    if (viewRect.top() < sceneRect.top() + margin) {
        newTop = viewRect.top() - margin * 2;
        needsExpansion = true;
    }
    if (viewRect.right() > sceneRect.right() - margin) {
        newRight = viewRect.right() + margin * 2;
        needsExpansion = true;
    }
    if (viewRect.bottom() > sceneRect.bottom() - margin) {
        newBottom = viewRect.bottom() + margin * 2;
        needsExpansion = true;
    }

    if (needsExpansion) {
        QRectF newSceneRect(newLeft, newTop,
                           newRight - newLeft,
                           newBottom - newTop);
        scene()->setSceneRect(newSceneRect);
    }
}

void SequenceRelationView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && (event->button() == Qt::MiddleButton ||
                      (event->button() == Qt::LeftButton && ((event->modifiers() & Qt::AltModifier) || m_spacePanning)))) {
        m_panning = false;
        setCursor(m_spacePanning ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }

    // Handle stopping of panning for left mouse button on empty space
    if (m_panning && event->button() == Qt::LeftButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor); // Restore default cursor
        event->accept();
        return;
    }

    if (m_connectSource && event->button() == Qt::RightButton) {
        cancelConnection();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void SequenceRelationView::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        QGraphicsView::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Space) {
        m_spacePanning = true;
        if (!m_panning) {
            setCursor(Qt::OpenHandCursor);
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_0 && event->modifiers().testFlag(Qt::ControlModifier)) {
        resetZoom();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_F) {
        focusOnSelection();
        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

void SequenceRelationView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        QGraphicsView::keyReleaseEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Space) {
        m_spacePanning = false;
        if (!m_panning) {
            setCursor(Qt::ArrowCursor);
        }
        event->accept();
        return;
    }

    QGraphicsView::keyReleaseEvent(event);
}

void SequenceRelationView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Check if we double-clicked on a node
    QPointF scenePos = mapToScene(event->pos());
    QGraphicsItem *item = m_scene->itemAt(scenePos, transform());
    
    if (item && item->type() == SequenceNodeItem::Type) {
        SequenceNodeItem *nodeItem = static_cast<SequenceNodeItem*>(item);
        emit sequenceSelected(nodeItem->name());
    } else {
        fitToContents();
    }
    
    QGraphicsView::mouseDoubleClickEvent(event);
}

void SequenceRelationView::contextMenuEvent(QContextMenuEvent *event)
{
    m_lastContextScenePos = mapToScene(event->pos());
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
    QPointF scenePos = mapToScene(pos);
    QGraphicsItem *item = m_scene->itemAt(scenePos, transform());
    SequenceNodeItem *nodeItem = (item && item->type() == SequenceNodeItem::Type)
                                     ? static_cast<SequenceNodeItem*>(item)
                                     : nullptr;
    SequenceEdgeItem *edgeItem = (item && item->type() == SequenceEdgeItem::Type)
                                     ? static_cast<SequenceEdgeItem*>(item)
                                     : nullptr;

    QMenu menu(this);
    
    QAction *fitAction = menu.addAction(tr("Поместить всё"));
    QAction *focusAction = menu.addAction(tr("Показать выделенное"));
    QAction *resetAction = menu.addAction(tr("Сбросить масштаб"));
    QAction *refreshAction = menu.addAction(tr("Обновить граф"));
    QAction *createAction = nullptr;
    if (!m_currentTemplateDirPath.isEmpty()) {
        createAction = menu.addAction(tr("Создать последовательность здесь"));
    }

    if (nodeItem) {
        menu.addSeparator();
        QAction *openAction = menu.addAction(tr("Открыть/редактировать"));
        QAction *connectAction = menu.addAction(tr("Начать связь отсюда"));
        connectAction->setEnabled(!m_currentTemplateDirPath.isEmpty());
        QAction *deleteAction = nullptr;
        if (!m_currentTemplateDirPath.isEmpty()) {
            deleteAction = menu.addAction(tr("Удалить из шаблона"));
        }

        QAction *nodeChoice = menu.exec(mapToGlobal(pos));
        if (nodeChoice == fitAction) {
            fitToContents();
        } else if (nodeChoice == focusAction) {
            focusOnSelection();
        } else if (nodeChoice == resetAction) {
            resetZoom();
        } else if (nodeChoice == refreshAction) {
            if (m_currentTemplateDirPath.isEmpty()) {
                rebuildGraph();
            } else {
                rebuildGraphForTemplate(m_currentTemplateDirPath);
            }
        } else if (nodeChoice == createAction && createAction) {
            QString name = QInputDialog::getText(this, tr("Новая последовательность"),
                                                 tr("Название последовательности:"));
            if (!name.isEmpty()) {
                if (m_templateMgr && m_templateMgr->createNewSequenceInTemplate(m_currentTemplateDirPath, name, QString())) {
                    rebuildGraphForTemplate(m_currentTemplateDirPath);
                    if (m_nodeItems.contains(name)) {
                        m_nodeItems[name]->setPos(m_lastContextScenePos);
                        savePositions();
                    }
                } else {
                    QMessageBox::warning(this, tr("Не создано"), tr("Не удалось создать последовательность."));
                }
            }
        } else if (nodeChoice == openAction) {
            emit sequenceSelected(nodeItem->name());
        } else if (nodeChoice == connectAction) {
            startConnectionFrom(nodeItem);
        } else if (nodeChoice == deleteAction && deleteAction) {
            if (QMessageBox::question(this, tr("Удалить последовательность"),
                                      tr("Удалить \"%1\" из шаблона?").arg(nodeItem->name()))
                == QMessageBox::Yes) {
                QString base = m_sequenceToBaseName.value(nodeItem->name());
                QString fileName = base.isEmpty() ? QString() : base + ".seq.json";
                QString absPath = fileName.isEmpty() ? QString() : QDir(m_currentTemplateDirPath).filePath(fileName);

                SolutionTemplate tmpl;
                if (loadCurrentTemplate(tmpl)) {
                    QVector<QString> updatedSeqs;
                    for (const QString &seqPath : tmpl.sequences()) {
                        if (seqPath == fileName || seqPath == absPath)
                            continue;
                        updatedSeqs.append(seqPath);
                    }

                    tmpl.clearSequences();
                    for (const QString &s : updatedSeqs) {
                        tmpl.addSequence(s);
                    }

                    if (tmpl.mainSequencePath() == fileName || tmpl.mainSequencePath() == absPath) {
                        tmpl.setMainSequencePath(QString());
                    }

                    QJsonObject targets = tmpl.getSequenceTargets();
                    if (!base.isEmpty()) {
                        targets.remove(base);
                        for (auto it = targets.begin(); it != targets.end();) {
                            QJsonArray arr = it.value().toArray();
                            QJsonArray filtered;
                            for (const QJsonValue &val : arr) {
                                if (val.toString() != base) {
                                    filtered.append(val);
                                }
                            }
                            if (filtered.isEmpty()) {
                                it = targets.erase(it);
                            } else {
                                it.value() = filtered;
                                ++it;
                            }
                        }
                    }
                    tmpl.setSequenceTargets(targets);

                    QMap<QString, QPointF> positions = tmpl.positions();
                    positions.remove(nodeItem->name());
                    tmpl.clearPositions();
                    for (auto it = positions.begin(); it != positions.end(); ++it) {
                        tmpl.setPositionForSequence(it.key(), it.value());
                    }

                    if (saveTemplate(tmpl)) {
                        if (!absPath.isEmpty() && QFile::exists(absPath)) {
                            QFile::remove(absPath);
                        }
                        rebuildGraphForTemplate(m_currentTemplateDirPath);
                    }
                }
            }
        }
        return;
    }

    if (edgeItem) {
        menu.addSeparator();
        QAction *removeEdgeAction = menu.addAction(tr("Удалить связь"));
        removeEdgeAction->setEnabled(!m_currentTemplateDirPath.isEmpty());
        QAction *choice = menu.exec(mapToGlobal(pos));
        if (choice == removeEdgeAction) {
            removeDirectedConnection(edgeItem->source()->name(), edgeItem->dest()->name());
        } else if (choice == fitAction) {
            fitToContents();
        } else if (choice == focusAction) {
            focusOnSelection();
        } else if (choice == resetAction) {
            resetZoom();
        } else if (choice == refreshAction) {
            if (m_currentTemplateDirPath.isEmpty()) {
                rebuildGraph();
            } else {
                rebuildGraphForTemplate(m_currentTemplateDirPath);
            }
        } else if (choice == createAction && createAction) {
            QString name = QInputDialog::getText(this, tr("Новая последовательность"),
                                                 tr("Название последовательности:"));
            if (!name.isEmpty()) {
                if (m_templateMgr && m_templateMgr->createNewSequenceInTemplate(m_currentTemplateDirPath, name, QString())) {
                    rebuildGraphForTemplate(m_currentTemplateDirPath);
                    if (m_nodeItems.contains(name)) {
                        m_nodeItems[name]->setPos(m_lastContextScenePos);
                        savePositions();
                    }
                } else {
                    QMessageBox::warning(this, tr("Не создано"), tr("Не удалось создать последовательность."));
                }
            }
        }
        return;
    }

    QAction *selectedAction = menu.exec(mapToGlobal(pos));
    
    if (selectedAction == fitAction) {
        fitToContents();
    } else if (selectedAction == focusAction) {
        focusOnSelection();
    } else if (selectedAction == resetAction) {
        resetZoom();
    } else if (selectedAction == refreshAction) {
        rebuildGraph();
    } else if (selectedAction == createAction && createAction) {
        QString name = QInputDialog::getText(this, tr("Новая последовательность"),
                                             tr("Название последовательности:"));
        if (!name.isEmpty()) {
            if (m_templateMgr && m_templateMgr->createNewSequenceInTemplate(m_currentTemplateDirPath, name, QString())) {
                rebuildGraphForTemplate(m_currentTemplateDirPath);
                if (m_nodeItems.contains(name)) {
                    m_nodeItems[name]->setPos(m_lastContextScenePos);
                    savePositions();
                }
            } else {
                QMessageBox::warning(this, tr("Не создано"), tr("Не удалось создать последовательность."));
            }
        }
    }
}

QRectF SequenceRelationView::contentBounds() const
{
    QRectF bounds;
    for (SequenceNodeItem *item : m_nodeItems) {
        if (item) {
            bounds = bounds.united(item->sceneBoundingRect());
        }
    }

    if (bounds.isNull() && m_scene) {
        bounds = m_scene->itemsBoundingRect();
    }

    return bounds;
}

void SequenceRelationView::focusOnSelection()
{
    QRectF bounds;
    for (SequenceNodeItem *item : m_nodeItems) {
        if (item && item->isSelected()) {
            bounds = bounds.united(item->sceneBoundingRect());
        }
    }

    if (bounds.isNull() || !bounds.isValid()) {
        fitToContents();
        return;
    }

    bounds = bounds.adjusted(-60, -60, 60, 60);
    fitInView(bounds, Qt::KeepAspectRatio);
    m_totalScale = transform().m11();
}

bool SequenceRelationView::addDirectedConnection(const QString &sourceName, const QString &targetName)
{
    if (sourceName.isEmpty() || targetName.isEmpty() || sourceName == targetName)
        return false;

    qDebug() << "[SequenceRelationView] Adding connection" << sourceName << "->" << targetName << "templateDir" << m_currentTemplateDirPath;

    QString sourceBase = resolveBaseName(sourceName);
    QString targetBase = resolveBaseName(targetName);

    SolutionTemplate tmpl;
    if (!loadCurrentTemplate(tmpl)) {
        qDebug() << "[SequenceRelationView] Cannot load current template for connection";
        return false;
    }

    QJsonObject targets = tmpl.getSequenceTargets();
    QJsonArray arr = targets.value(sourceBase).toArray();
    bool added = false;
    if (!arr.contains(targetBase)) {
        arr.append(targetBase);
        added = true;
    }
    targets[sourceBase] = arr;
    tmpl.setSequenceTargets(targets);
    if (saveTemplate(tmpl)) {
        // show edge immediately even if it already existed in file
        addEdge(sourceName, targetName);
        if (!m_currentTemplateDirPath.isEmpty()) {
            m_nodePositions.insert(sourceName, m_nodeItems.value(sourceName)->pos());
            m_nodePositions.insert(targetName, m_nodeItems.value(targetName)->pos());
        }
        // Refresh template cache so further operations stay in sync
        if (m_templateMgr) {
            m_templateMgr->refresh();
        }
        // Rebuild to ensure on-disk data is in sync and edges visible
        rebuildGraphForTemplate(m_currentTemplateDirPath);
        return true;
    }

    qDebug() << "[SequenceRelationView] Failed to save template when adding connection" << sourceName << "->" << targetName;
    return false;
}

bool SequenceRelationView::removeDirectedConnection(const QString &sourceName, const QString &targetName)
{
    QString sourceBase = resolveBaseName(sourceName);
    QString targetBase = resolveBaseName(targetName);
    if (sourceBase.isEmpty() || targetBase.isEmpty())
        return false;

    SolutionTemplate tmpl;
    if (!loadCurrentTemplate(tmpl))
        return false;

    QJsonObject targets = tmpl.getSequenceTargets();
    if (!targets.contains(sourceBase))
        return false;

    QJsonArray arr = targets.value(sourceBase).toArray();
    QJsonArray updated;
    for (const QJsonValue &val : arr) {
        if (val.toString() != targetBase) {
            updated.append(val);
        }
    }

    if (updated.isEmpty()) {
        targets.remove(sourceBase);
    } else {
        targets[sourceBase] = updated;
    }

    tmpl.setSequenceTargets(targets);
    if (saveTemplate(tmpl)) {
        removeEdgeItem(sourceName, targetName);
        return true;
    }

    return false;
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

void SequenceRelationView::setEdgesFromTemplate(const QJsonObject &sequenceTargets, const QMap<QString, QString> &baseNameToSequenceNameMap)
{
    for (auto it = sequenceTargets.constBegin(); it != sequenceTargets.constEnd(); ++it) {
        QString sourceBaseName = it.key();
        QJsonArray targetArray = it.value().toArray();

        QString sourceSequenceName = baseNameToSequenceNameMap.value(sourceBaseName, sourceBaseName);

        for (const QJsonValue &targetValue : targetArray) {
            QString targetBaseName = targetValue.toString();
            QString targetSequenceName = baseNameToSequenceNameMap.value(targetBaseName, targetBaseName);

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

bool SequenceRelationView::loadCurrentTemplate(SolutionTemplate &tmpl) const
{
    if (m_currentTemplateDirPath.isEmpty())
        return false;

    QString templateJsonPath = QDir(m_currentTemplateDirPath).filePath("template.templ.json");
    QFile file(templateJsonPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return false;

    tmpl = SolutionTemplate::fromJson(doc.object());
    // Ensure sequenceTargets object exists (even if empty)
    if (!tmpl.getSequenceTargets().isEmpty()) {
        // already present
    }
    return true;
}

QString SequenceRelationView::resolveBaseName(const QString &sequenceName)
{
    // Fast path: map populated during graph build
    if (m_sequenceToBaseName.contains(sequenceName)) {
        return m_sequenceToBaseName.value(sequenceName);
    }

    // Try to derive from sequence manager (if present) by matching names and template path
    if (m_seqMgr && !m_currentTemplateDirPath.isEmpty()) {
        const QMap<QString, PromptSequence> &seqs = m_seqMgr->sequences();
        for (auto it = seqs.constBegin(); it != seqs.constEnd(); ++it) {
            if (it.value().name() == sequenceName) {
                QFileInfo fi(it.key());
                if (fi.absolutePath() == m_currentTemplateDirPath) {
                    QString base = fi.baseName();
                    m_sequenceToBaseName.insert(sequenceName, base);
                    return base;
                }
            }
        }
    }

    // As a fallback, scan template directory for a file with matching sequence name
    if (!m_currentTemplateDirPath.isEmpty()) {
        QDir dir(m_currentTemplateDirPath);
        QStringList filters{"*.seq.json"};
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks);
        for (const QFileInfo &fi : files) {
            QFile f(fi.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly))
                continue;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            if (!doc.isObject())
                continue;
            PromptSequence seq = PromptSequence::fromJson(doc.object());
            if (seq.name() == sequenceName) {
                QString base = fi.baseName();
                m_sequenceToBaseName.insert(sequenceName, base);
                return base;
            }
        }
    }

    // Fallback to using the sequence name itself
    return sequenceName;
}

bool SequenceRelationView::saveTemplate(const SolutionTemplate &tmpl)
{
    QString templateJsonPath = QDir(m_currentTemplateDirPath).filePath("template.templ.json");

    if (m_templateMgr) {
        if (m_templateMgr->save(tmpl, m_currentTemplateDirPath)) {
            return true;
        }
        qDebug() << "[SequenceRelationView] TemplateManager save failed, fallback to manual write";
    }

    if (m_currentTemplateDirPath.isEmpty())
        return false;

    QFile file(templateJsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(QJsonDocument(tmpl.toJson()).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void SequenceRelationView::resetZoom()
{
    if (!m_scene || m_scene->items().isEmpty()) {
        resetTransform();
        m_totalScale = 1.0;
        return;
    }

    fitToContents();
}

void SequenceRelationView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, backgroundBrush());

    const int gridSize = 40;
    const int boldStep = 5;
    QPen lightPen(QColor(230, 230, 240));
    lightPen.setWidth(1);
    QPen boldPen(QColor(210, 210, 220));
    boldPen.setWidth(1);

    int left = static_cast<int>(qFloor(rect.left() / gridSize)) * gridSize;
    int right = static_cast<int>(qCeil(rect.right() / gridSize)) * gridSize;
    int top = static_cast<int>(qFloor(rect.top() / gridSize)) * gridSize;
    int bottom = static_cast<int>(qCeil(rect.bottom() / gridSize)) * gridSize;

    int index = 0;
    for (int x = left; x <= right; x += gridSize, ++index) {
        painter->setPen((index % boldStep == 0) ? boldPen : lightPen);
        painter->drawLine(x, top, x, bottom);
    }

    index = 0;
    for (int y = top; y <= bottom; y += gridSize, ++index) {
        painter->setPen((index % boldStep == 0) ? boldPen : lightPen);
        painter->drawLine(left, y, right, y);
    }
}

void SequenceRelationView::startConnectionFrom(SequenceNodeItem *source)
{
    if (!source || m_currentTemplateDirPath.isEmpty())
        return;
    cancelConnection();
    m_connectSource = source;
    m_connecting = true;
    if (m_connectSource) {
        m_connectSource->setConnectionSource(true);
    }
    if (m_scene) {
        QPointF start = source->sceneBoundingRect().center();
        QPen pen(QColor(90, 90, 90, 180), 2, Qt::DashLine);
        pen.setCapStyle(Qt::RoundCap);
        m_tempConnectionLine = m_scene->addLine(QLineF(start, start), pen);
    }
    setCursor(Qt::CrossCursor);
}

void SequenceRelationView::updateTempConnectionTo(const QPointF &scenePos)
{
    if (m_tempConnectionLine && m_connectSource) {
        QPointF start = m_connectSource->sceneBoundingRect().center();
        m_tempConnectionLine->setLine(QLineF(start, scenePos));
    }
}

void SequenceRelationView::cancelConnection()
{
    if (m_tempConnectionLine) {
        QGraphicsScene *lineScene = m_tempConnectionLine->scene();
        if (lineScene) {
            lineScene->removeItem(m_tempConnectionLine);
        }
        delete m_tempConnectionLine;
        m_tempConnectionLine = nullptr;
    }
    if (m_connectSource) {
        if (m_connectSource->scene()) {
            m_connectSource->setConnectionSource(false);
        }
    }
    for (SequenceNodeItem *node : m_nodeItems) {
        if (node) node->setConnectionTargetHover(false);
    }
    m_connectSource = nullptr;
    m_connecting = false;
    if (!m_panning && !m_spacePanning) {
        setCursor(Qt::ArrowCursor);
    }
}


