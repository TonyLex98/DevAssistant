#include "ui/graph/SequenceRelationView.h"
#include "ui/graph/SequenceGraphLayout.h"
#include "ui/graph/SequenceGraphModel.h"
#include "core/workflow/SolutionTemplate.h"
#include "core/workflow/SolutionTemplateManager.h"
#include "core/util/Logging.h"
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QtMath>

namespace {
// Ctrl+wheel zoom: each notch scales by kZoomStep, within these limits.
constexpr qreal kMinScale = 0.25;
constexpr qreal kMaxScale = 6.0;
constexpr qreal kZoomStep = 1.15;
} // namespace

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

    // Zoom anchors are set per wheel event; scrolling is done by hand while panning.
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void SequenceRelationView::setSequenceManager(PromptSequenceManager *seqMgr)
{
    qCDebug(logViewer) << "[SequenceRelationView] setSequenceManager called";
    m_seqMgr = seqMgr;
    // No rebuild here: the caller restores the layout first and then rebuilds when it is ready.
}

void SequenceRelationView::setTemplateManager(SolutionTemplateManager *tmplMgr)
{
    qCDebug(logViewer) << "[SequenceRelationView] setTemplateManager called";
    m_templateMgr = tmplMgr;
}

void SequenceRelationView::rebuildGraph()
{
    qCDebug(logViewer) << "[SequenceRelationView] rebuildGraph() called";
    // Placing the nodes moves them; that is not the user moving them, so nothing is saved.
    const QScopedValueRollback<bool> rebuilding(m_rebuilding, true);

    const QTransform prevTransform = transform();
    const QPointF prevCenter = mapToScene(viewport()->rect().center());
    const bool hadViewState = !m_scene->items().isEmpty();

    const QMap<QString, QPointF> savedPositions = m_nodePositions;
    qCDebug(logViewer) << "[SequenceRelationView] Saved positions before clearing:" << savedPositions;

    clearScenePreservingPositions();

    if (!m_seqMgr) {
        qCDebug(logViewer) << "[SequenceRelationView] No sequence manager, returning";
        return;
    }

    const QMap<QString, PromptSequence> &sequences = m_seqMgr->sequences();
    if (sequences.isEmpty()) {
        qCDebug(logViewer) << "[SequenceRelationView] No sequences, returning";
        return;
    }

    qCDebug(logViewer) << "[SequenceRelationView] Adding nodes for sequences:" << sequences.keys();
    addSequenceNodes(SequenceGraphModel::entriesFromSequences(sequences));
    addLinks(SequenceGraphModel::linksForSequences(sequences));

    // Without a template there is no file with positions, only those kept from this session.
    qCDebug(logViewer) << "[SequenceRelationView] Restoring saved positions after adding nodes";
    for (auto it = savedPositions.constBegin(); it != savedPositions.constEnd(); ++it) {
        if (placeNode(it.key(), it.value())) {
            qCDebug(logViewer) << "[SequenceRelationView] Restored position (from internal) for node:" << it.key() << "at" << it.value();
        }
    }

    finishRebuild(prevTransform, prevCenter, hadViewState);
    qCDebug(logViewer) << "[SequenceRelationView] rebuildGraph() completed";
}

void SequenceRelationView::rebuildGraphForTemplate(const QString &templateDirPath)
{
    qCDebug(logViewer) << "[SequenceRelationView] rebuildGraphForTemplate called for:" << templateDirPath;
    const QScopedValueRollback<bool> rebuilding(m_rebuilding, true);
    const QTransform prevTransform = transform();
    const QPointF prevCenter = mapToScene(viewport()->rect().center());
    const bool hadViewState = !m_scene->items().isEmpty();

    const QMap<QString, QPointF> savedPositions = m_nodePositions;
    qCDebug(logViewer) << "[SequenceRelationView] Saved positions before clearing:" << savedPositions;

    clearScenePreservingPositions();

    // Set before the nodes are added, so their positions go into this template.
    setCurrentTemplateDir(templateDirPath);

    if (!QDir(templateDirPath).exists()) {
        qCDebug(logViewer) << "[SequenceRelationView] Template directory does not exist:" << templateDirPath;
        return;
    }

    const QVector<SequenceGraphEntry> entries = SequenceGraphModel::entriesFromTemplateDir(templateDirPath);
    addSequenceNodes(entries);
    addLinks(SequenceGraphModel::linksForTemplateDir(templateDirPath, entries));

    qCDebug(logViewer) << "[SequenceRelationView] Restoring saved positions after adding nodes for template";
    if (m_templateMgr) {
        const QMap<QString, QPointF> templatePositions = m_templateMgr->loadPositionsFromTemplate(templateDirPath);
        for (auto it = templatePositions.constBegin(); it != templatePositions.constEnd(); ++it) {
            if (placeNode(it.key(), it.value())) {
                qCDebug(logViewer) << "[SequenceRelationView] Loaded position from template for node:" << it.key() << "at" << it.value();
            }
        }
    }

    // Positions from this session can be newer than the file, so they win where the two differ.
    for (auto it = savedPositions.constBegin(); it != savedPositions.constEnd(); ++it) {
        const QString &nodeName = it.key();
        const QPointF &pos = it.value();
        if (!m_nodeItems.contains(nodeName))
            continue;
        if (!m_nodePositions.contains(nodeName) || m_nodePositions.value(nodeName) != pos) {
            placeNode(nodeName, pos);
            qCDebug(logViewer) << "[SequenceRelationView] Restored position (from internal, possibly more recent) for node:" << nodeName << "at" << pos;
        } else {
            qCDebug(logViewer) << "[SequenceRelationView] Skipping internal position for node:" << nodeName << ", already loaded from template";
        }
    }

    finishRebuild(prevTransform, prevCenter, hadViewState);
    qCDebug(logViewer) << "[SequenceRelationView] rebuildGraphForTemplate() completed for:" << templateDirPath;
}

void SequenceRelationView::addSequenceNodes(const QVector<SequenceGraphEntry> &entries)
{
    for (const SequenceGraphEntry &entry : entries) {
        m_sequenceToBaseName.insert(entry.sequence.name(), entry.baseName);
        addNode(entry.sequence.name(), entry.sequence.description());
    }
}

void SequenceRelationView::addLinks(const QVector<SequenceGraphLink> &links)
{
    for (const SequenceGraphLink &link : links) {
        addEdge(link.from, link.to);
    }
}

void SequenceRelationView::addNode(const QString &name, const QString &description)
{
    qCDebug(logViewer) << "[SequenceRelationView] addNode called for:" << name << "with current m_nodePositions size:" << m_nodePositions.size();

    if (m_nodeItems.contains(name)) {
        qCDebug(logViewer) << "[SequenceRelationView] Node" << name << "already exists, skipping";
        return;
    }

    SequenceNodeItem *item = new SequenceNodeItem(name, description);
    m_scene->addItem(item);
    m_nodeItems[name] = item;

    if (m_nodePositions.contains(name)) {
        QPointF pos = m_nodePositions[name];
        item->setPos(pos);
        qCDebug(logViewer) << "[SequenceRelationView] Set position for node:" << name << "at" << pos;
    } else {
        qCDebug(logViewer) << "[SequenceRelationView] No saved position for node:" << name << ", keeping default position";
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

// Moving a node also saves positions through the node, so the map is updated after setPos.
bool SequenceRelationView::placeNode(const QString &name, const QPointF &pos)
{
    SequenceNodeItem *node = m_nodeItems.value(name);
    if (!node)
        return false;
    node->setPos(pos);
    m_nodePositions[name] = pos;
    return true;
}

void SequenceRelationView::finishRebuild(const QTransform &prevTransform, const QPointF &prevCenter, bool hadViewState)
{
    if (m_nodePositions.isEmpty()) {
        qCDebug(logViewer) << "[SequenceRelationView] No saved positions found, using default layout";
        positionNodes();
    } else {
        qCDebug(logViewer) << "[SequenceRelationView] Saved positions restored, nodes in scene:" << m_nodeItems.size();
    }

    updateAllEdges();

    // A refresh keeps the user's zoom and scroll; only a graph shown for the first time is fitted.
    if (hadViewState && !m_scene->items().isEmpty()) {
        setTransform(prevTransform);
        centerOn(prevCenter);
        m_totalScale = transform().m11();
    } else {
        fitToContents();
    }
}

void SequenceRelationView::positionNodes()
{
    if (m_nodeItems.isEmpty()) return;

    QVector<qreal> heights;
    heights.reserve(m_nodeItems.size());
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        heights.append(it.value()->boundingRect().height());
    }

    const QVector<QPointF> positions = SequenceGraphLayout::columnPositions(heights);
    auto pos = positions.constBegin();
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it, ++pos) {
        it.value()->setPos(*pos);
    }

    updateAllEdges();
    m_scene->setSceneRect(SequenceGraphLayout::sceneRectAround(nodesBounds(false)));
}

void SequenceRelationView::updateAllEdges()
{
    for (SequenceEdgeItem *edge : m_edgeItems) {
        if (edge && m_scene && m_scene->items().contains(edge)) {
            edge->updatePosition();
        }
    }
}

void SequenceRelationView::updateEdgesForNode(SequenceNodeItem *node)
{
    if (!node || !m_nodeEdges.contains(node))
        return;

    // The scene check skips links that were deleted with an earlier scene.
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

void SequenceRelationView::fitToContents()
{
    QRectF bounds = contentBounds();
    if (bounds.isNull() || !bounds.isValid())
        return;

    const qreal margin = SequenceGraphLayout::kFitMargin;
    bounds = bounds.adjusted(-margin, -margin, margin, margin);
    fitInView(bounds, Qt::KeepAspectRatio);
    const qreal fittedScale = transform().m11();
    const qreal readable = SequenceGraphLayout::readableScale(fittedScale);
    if (!qFuzzyCompare(fittedScale, readable)) {
        setTransform(QTransform::fromScale(readable, readable));
        centerOn(bounds.center());
    }
    m_totalScale = transform().m11();
}

void SequenceRelationView::clearScenePreservingPositions()
{
    // The temporary link line belongs to the scene, so drop it before the scene deletes it.
    cancelConnection();
    m_scene->clear();
    m_nodeItems.clear();
    m_edgeItems.clear();
    m_edgesSet.clear();
    m_nodeEdges.clear();  // the scene has deleted the edges
    m_sequenceToBaseName.clear();
}

void SequenceRelationView::wheelEvent(QWheelEvent *event)
{
    const bool zooming = event->modifiers().testFlag(Qt::ControlModifier);
    if (!zooming) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    // Touchpads may report only pixel deltas.
    qreal delta = event->angleDelta().y();
    if (qFuzzyIsNull(delta) && !event->pixelDelta().isNull()) {
        delta = event->pixelDelta().y();
    }

    if (qFuzzyIsNull(delta)) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    qreal steps = delta / 120.0; // 120 == one mouse wheel step
    qreal factor = qPow(kZoomStep, steps);
    qreal targetScale = qBound(kMinScale, m_totalScale * factor, kMaxScale);
    qreal actualFactor = targetScale / m_totalScale;

    if (qFuzzyCompare(actualFactor, 1.0)) {
        event->accept();
        return;
    }

    // Zoom around the cursor, then go back to the anchor that manual panning relies on.
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scale(actualFactor, actualFactor);
    setTransformationAnchor(QGraphicsView::NoAnchor);

    m_totalScale = targetScale;

    expandSceneIfNeeded();
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
        SequenceNodeItem *targetNode = nodeAt(scenePos, Qt::DescendingOrder, m_connectSource);
        if (targetNode) {
            if (!addDirectedConnection(m_connectSource->name(), targetNode->name())) {
                QMessageBox::warning(this, tr("Link Not Created"),
                                     tr("Could not create a link between %1 and %2.")
                                         .arg(m_connectSource->name(), targetNode->name()));
            }
            cancelConnection();
        } else {
            // A click on empty space keeps the link mode, so the user can still pick a target.
            updateTempConnectionTo(scenePos);
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // With Shift held, pick the bottom-most node, to reach one hidden under another.
        Qt::SortOrder order = event->modifiers() & Qt::ShiftModifier ? Qt::AscendingOrder : Qt::DescendingOrder;
        SequenceNodeItem *targetNode = nodeAt(mapToScene(event->pos()), order);

        if (targetNode) {
            setCurrentSequence(targetNode->name());
        } else {
            // Dragging empty space pans the view.
            m_panning = true;
            m_lastPanPoint = event->pos();
            setCursor(Qt::ClosedHandCursor);
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

void SequenceRelationView::expandSceneIfNeeded()
{
    const QRectF visibleRect = mapToScene(rect()).boundingRect();
    QRectF expanded;
    if (SequenceGraphLayout::expandedSceneRect(scene()->sceneRect(), visibleRect, &expanded)) {
        scene()->setSceneRect(expanded);
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

    // Panning that started on empty space.
    if (m_panning && event->button() == Qt::LeftButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
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
    // Remembered so "Create Sequence Here" can place the new node where the menu was opened.
    m_lastContextScenePos = mapToScene(event->pos());
    showContextMenu(event->pos());
}

void SequenceRelationView::setCurrentSequence(const QString &sequenceName)
{
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        it.value()->setHighlighted(false);
    }

    SequenceNodeItem *currentItem = m_nodeItems.value(sequenceName);
    if (currentItem) {
        currentItem->setHighlighted(true);
    }
}

void SequenceRelationView::showContextMenu(const QPoint &pos)
{
    QGraphicsItem *item = m_scene->itemAt(mapToScene(pos), transform());
    SequenceNodeItem *nodeItem = (item && item->type() == SequenceNodeItem::Type)
                                     ? static_cast<SequenceNodeItem*>(item)
                                     : nullptr;
    SequenceEdgeItem *edgeItem = (item && item->type() == SequenceEdgeItem::Type)
                                     ? static_cast<SequenceEdgeItem*>(item)
                                     : nullptr;
    // Editing actions need a template file to write to.
    const bool templateOpen = !m_currentTemplateDirPath.isEmpty();

    QMenu menu(this);
    QAction *fitAction = menu.addAction(tr("Fit All"));
    QAction *focusAction = menu.addAction(tr("Show Selection"));
    QAction *resetAction = menu.addAction(tr("Reset Zoom"));
    QAction *refreshAction = menu.addAction(tr("Refresh Graph"));
    QAction *createAction = templateOpen ? menu.addAction(tr("Create Sequence Here")) : nullptr;

    QAction *openAction = nullptr;
    QAction *connectAction = nullptr;
    QAction *deleteAction = nullptr;
    QAction *removeEdgeAction = nullptr;
    if (nodeItem) {
        menu.addSeparator();
        openAction = menu.addAction(tr("Open/Edit"));
        connectAction = menu.addAction(tr("Start Link Here"));
        connectAction->setEnabled(templateOpen);
        if (templateOpen) {
            deleteAction = menu.addAction(tr("Remove from Template"));
        }
    } else if (edgeItem) {
        menu.addSeparator();
        removeEdgeAction = menu.addAction(tr("Delete Link"));
        removeEdgeAction->setEnabled(templateOpen);
    }

    QAction *choice = menu.exec(mapToGlobal(pos));
    if (!choice)
        return;

    if (choice == fitAction) {
        fitToContents();
    } else if (choice == focusAction) {
        focusOnSelection();
    } else if (choice == resetAction) {
        resetZoom();
    } else if (choice == refreshAction) {
        // With a template open, refreshing keeps showing that template.
        if (templateOpen) {
            rebuildGraphForTemplate(m_currentTemplateDirPath);
        } else {
            rebuildGraph();
        }
    } else if (choice == createAction) {
        createSequenceAtContextPos();
    } else if (choice == openAction) {
        emit sequenceSelected(nodeItem->name());
    } else if (choice == connectAction) {
        startConnectionFrom(nodeItem);
    } else if (choice == deleteAction) {
        if (QMessageBox::question(this, tr("Delete Sequence"),
                                  tr("Remove \"%1\" from the template?").arg(nodeItem->name()))
            == QMessageBox::Yes) {
            removeSequenceFromTemplate(nodeItem->name());
        }
    } else if (choice == removeEdgeAction) {
        removeDirectedConnection(edgeItem->source()->name(), edgeItem->dest()->name());
    }
}

void SequenceRelationView::createSequenceAtContextPos()
{
    QString name = QInputDialog::getText(this, tr("New Sequence"),
                                         tr("Sequence name:"));
    if (name.isEmpty())
        return;

    if (m_templateMgr && m_templateMgr->createNewSequenceInTemplate(m_currentTemplateDirPath, name, QString())) {
        rebuildGraphForTemplate(m_currentTemplateDirPath);
        if (m_nodeItems.contains(name)) {
            m_nodeItems[name]->setPos(m_lastContextScenePos);
            savePositions();
        }
    } else {
        QMessageBox::warning(this, tr("Not Created"), tr("Could not create the sequence."));
    }
}

void SequenceRelationView::removeSequenceFromTemplate(const QString &sequenceName)
{
    const QString base = m_sequenceToBaseName.value(sequenceName);

    SolutionTemplate tmpl;
    if (!SequenceGraphModel::loadTemplate(m_currentTemplateDirPath, tmpl))
        return;

    SequenceGraphModel::removeSequence(tmpl, sequenceName, base, m_currentTemplateDirPath);
    if (!SequenceGraphModel::saveTemplate(tmpl, m_currentTemplateDirPath, m_templateMgr))
        return;

    // The file goes only after the template stopped referring to it.
    const QString absPath = SequenceGraphModel::sequenceFilePath(m_currentTemplateDirPath, base);
    if (!absPath.isEmpty() && QFile::exists(absPath)) {
        QFile::remove(absPath);
    }
    rebuildGraphForTemplate(m_currentTemplateDirPath);
}

QRectF SequenceRelationView::nodesBounds(bool selectedOnly) const
{
    QRectF bounds;
    for (SequenceNodeItem *item : m_nodeItems) {
        if (item && (!selectedOnly || item->isSelected())) {
            bounds = bounds.united(item->sceneBoundingRect());
        }
    }
    return bounds;
}

QRectF SequenceRelationView::contentBounds() const
{
    QRectF bounds = nodesBounds(false);
    if (bounds.isNull() && m_scene) {
        bounds = m_scene->itemsBoundingRect();
    }
    return bounds;
}

void SequenceRelationView::focusOnSelection()
{
    QRectF bounds = nodesBounds(true);
    if (bounds.isNull() || !bounds.isValid()) {
        fitToContents();
        return;
    }

    const qreal margin = SequenceGraphLayout::kFocusMargin;
    bounds = bounds.adjusted(-margin, -margin, margin, margin);
    fitInView(bounds, Qt::KeepAspectRatio);
    m_totalScale = transform().m11();
}

SequenceNodeItem *SequenceRelationView::nodeAt(const QPointF &scenePos, Qt::SortOrder order,
                                               const SequenceNodeItem *ignored) const
{
    const QList<QGraphicsItem*> itemsAtPos = m_scene->items(scenePos, Qt::IntersectsItemShape, order, transform());
    for (QGraphicsItem *item : itemsAtPos) {
        if (item && item->type() == SequenceNodeItem::Type && item != ignored) {
            return static_cast<SequenceNodeItem*>(item);
        }
    }
    return nullptr;
}

bool SequenceRelationView::addDirectedConnection(const QString &sourceName, const QString &targetName)
{
    if (sourceName.isEmpty() || targetName.isEmpty() || sourceName == targetName)
        return false;

    qCDebug(logViewer) << "[SequenceRelationView] Adding connection" << sourceName << "->" << targetName << "templateDir" << m_currentTemplateDirPath;

    QString sourceBase = resolveBaseName(sourceName);
    QString targetBase = resolveBaseName(targetName);

    SolutionTemplate tmpl;
    if (!SequenceGraphModel::loadTemplate(m_currentTemplateDirPath, tmpl)) {
        qCDebug(logViewer) << "[SequenceRelationView] Cannot load current template for connection";
        return false;
    }

    SequenceGraphModel::addLink(tmpl, sourceBase, targetBase);
    if (!SequenceGraphModel::saveTemplate(tmpl, m_currentTemplateDirPath, m_templateMgr)) {
        qCDebug(logViewer) << "[SequenceRelationView] Failed to save template when adding connection" << sourceName << "->" << targetName;
        return false;
    }

    // Show the edge even if the file already had it.
    addEdge(sourceName, targetName);
    if (!m_currentTemplateDirPath.isEmpty()) {
        m_nodePositions.insert(sourceName, m_nodeItems.value(sourceName)->pos());
        m_nodePositions.insert(targetName, m_nodeItems.value(targetName)->pos());
    }
    // The manager caches templates; reload it so later edits start from the saved file.
    if (m_templateMgr) {
        m_templateMgr->refresh();
    }
    rebuildGraphForTemplate(m_currentTemplateDirPath);
    return true;
}

bool SequenceRelationView::removeDirectedConnection(const QString &sourceName, const QString &targetName)
{
    QString sourceBase = resolveBaseName(sourceName);
    QString targetBase = resolveBaseName(targetName);
    if (sourceBase.isEmpty() || targetBase.isEmpty())
        return false;

    SolutionTemplate tmpl;
    if (!SequenceGraphModel::loadTemplate(m_currentTemplateDirPath, tmpl))
        return false;

    if (!SequenceGraphModel::removeLink(tmpl, sourceBase, targetBase))
        return false;

    if (SequenceGraphModel::saveTemplate(tmpl, m_currentTemplateDirPath, m_templateMgr)) {
        removeEdgeItem(sourceName, targetName);
        return true;
    }

    return false;
}

void SequenceRelationView::saveNodePositions()
{
    qCDebug(logViewer) << "[SequenceRelationView] Saving node positions to internal map, number of nodes:" << m_nodeItems.size();

    // Rebuilt from scratch so nodes that are gone do not keep a stale position.
    m_nodePositions.clear();

    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        QString nodeName = it.key();
        SequenceNodeItem* node = it.value();

        if (node && m_scene && m_scene->items().contains(node)) {
            QPointF pos = node->pos();
            m_nodePositions[nodeName] = pos;
            qCDebug(logViewer) << "[SequenceRelationView] Saved position for node:" << nodeName << "at" << pos;
        } else {
            qCDebug(logViewer) << "[SequenceRelationView] Node" << nodeName << "not found in scene, skipping";
        }
    }
}

void SequenceRelationView::savePositionsToTemplate(const QString& templateDirPath)
{
    qCDebug(logViewer) << "[SequenceRelationView] Saving positions to template directory:" << templateDirPath;

    saveNodePositions();

    if (m_templateMgr) {
        m_templateMgr->savePositionsToTemplate(templateDirPath, m_nodePositions);
    } else {
        qCDebug(logViewer) << "[SequenceRelationView] No template manager available to save positions";
    }
}

void SequenceRelationView::savePositions()
{
    if (m_rebuilding)
        return;
    saveNodePositions();

    if (!m_currentTemplateDirPath.isEmpty() && m_templateMgr) {
        savePositionsToTemplate(m_currentTemplateDirPath);
    }
}

void SequenceRelationView::saveLayout()
{
    if (!m_currentTemplateDirPath.isEmpty() && m_templateMgr) {
        savePositionsToTemplate(m_currentTemplateDirPath);
    } else {
        saveNodePositions();
    }
}

void SequenceRelationView::restoreLayout()
{
    // Positions are read from the template file when the graph is built, and the session map
    // survives on its own, so there is nothing to load here; the slot stays for the callers.
    qCDebug(logViewer) << "[SequenceRelationView] restoreLayout() called";
    qCDebug(logViewer) << "[SequenceRelationView] restoreLayout() completed, positions in m_nodePositions:" << m_nodePositions.size();
}

void SequenceRelationView::setCurrentTemplateDir(const QString& templateDirPath)
{
    m_currentTemplateDirPath = templateDirPath;
    qCDebug(logViewer) << "[SequenceRelationView] Set current template directory to:" << templateDirPath;
}

QString SequenceRelationView::resolveBaseName(const QString &sequenceName)
{
    // Filled while the graph is built; the lookups below cover sequences added since then.
    if (m_sequenceToBaseName.contains(sequenceName)) {
        return m_sequenceToBaseName.value(sequenceName);
    }

    if (!m_currentTemplateDirPath.isEmpty()) {
        QString base;
        bool found = m_seqMgr && SequenceGraphModel::findBaseNameInSequences(
                                     m_seqMgr->sequences(), sequenceName, m_currentTemplateDirPath, &base);
        if (!found) {
            found = SequenceGraphModel::findBaseNameInDir(m_currentTemplateDirPath, sequenceName, &base);
        }
        if (found) {
            m_sequenceToBaseName.insert(sequenceName, base);
            return base;
        }
    }

    // Nothing matched, so the name itself serves as the key.
    return sequenceName;
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

    // Every fifth grid line is darker, to help judge distances when zoomed out.
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
    m_connectSource->setConnectionSource(true);
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
    if (m_connectSource && m_connectSource->scene()) {
        m_connectSource->setConnectionSource(false);
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
