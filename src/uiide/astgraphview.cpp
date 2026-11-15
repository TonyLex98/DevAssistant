#include "astgraphview.h"
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QMouseEvent>

ASTGraphView::ASTGraphView(QWidget *parent) : QGraphicsView(parent) {
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag); // Disable default drag mode to implement custom panning
}


// ---------- graph construction ----------
void ASTGraphView::setAST(const QJsonObject &ast) {
    rootAst_ = ast;
    scene_->clear();
    nodes.clear();
    nodeMap_.clear();

    QJsonArray children = ast["children"].toArray();
    int x = 20, y = 20;
    const int w = 220, h = 44, dx = 260, dy = 70;

    int id = 0;
    for (const auto &childVal : children) {
        QJsonObject c = childVal.toObject();
        QString type = c["type"].toString();
        if (type.contains("function") || type.contains("class") ||
            type.contains("struct") || type.contains("namespace")) {

            QString label = nodeLabel(c);

            auto *rect = scene_->addRect(x, y, w, h,
                                         QPen(Qt::black),
                                         QBrush(QColor(230, 244, 255)));
            auto *txt = scene_->addText(label);
            txt->setPos(x + 8, y + 8);

            Node n{ id++, QRectF(x, y, w, h) };
            n.start_row = c["start_row"].toInt();
            n.start_col = c["start_column"].toInt();
            nodes.push_back(n);

            nodeMap_[n.id] = c;

            x += dx;
            if (x > 1000) { x = 20; y += dy; }
        }
    }

    // Set scene with larger bounds to allow panning, but with a default size
    QRectF itemsRect = scene_->itemsBoundingRect();
    if (itemsRect.isEmpty()) {
        // Default scene size if no items
        scene_->setSceneRect(-500, -500, 1000, 1000);
    } else {
        // Use the bounding rect but ensure minimum size for panning
        QRectF adjustedRect = itemsRect.adjusted(-100, -100, 100, 100);
        // Ensure minimum size for panning
        qreal width = qMax(adjustedRect.width(), 1000.0);
        qreal height = qMax(adjustedRect.height(), 1000.0);
        qreal left = adjustedRect.left() - (width - adjustedRect.width()) / 2;
        qreal top = adjustedRect.top() - (height - adjustedRect.height()) / 2;
        scene_->setSceneRect(left, top, width, height);
    }
}

// ------------------------------------------------------------------
void ASTGraphView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPointF p = mapToScene(event->pos());
        bool nodeClicked = false;

        // Check if any node was clicked
        for (const auto &n : nodes) {
            if (n.rect.contains(p)) {
                nodeClicked = true;
                emit openAt(n.start_row, n.start_col);
                break;
            }
        }

        // If no node was clicked, start panning
        if (!nodeClicked) {
            m_dragging = true;
            m_lastPanPoint = event->pos();
            setCursor(Qt::ClosedHandCursor); // Change cursor to indicate panning
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void ASTGraphView::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        // Calculate the difference in mouse position
        QPoint delta = event->pos() - m_lastPanPoint;

        // Scroll the view by the delta
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());

        // Expand scene if we're panning near the edges
        expandSceneIfNeeded();

        // Update the last pan point
        m_lastPanPoint = event->pos();
    }
    QGraphicsView::mouseMoveEvent(event);
}

void ASTGraphView::expandSceneIfNeeded() {
    // Get current view area in scene coordinates
    QRectF viewRect = mapToScene(rect()).boundingRect();

    // Get current scene rectangle
    QRectF currentSceneRect = scene_->sceneRect();

    // Calculate margins - expand if view is near the edge of the scene
    qreal margin = 200.0; // pixels of margin

    qreal newLeft = currentSceneRect.left();
    qreal newTop = currentSceneRect.top();
    qreal newRight = currentSceneRect.right();
    qreal newBottom = currentSceneRect.bottom();

    // Check if we need to expand in any direction
    bool needsExpansion = false;

    if (viewRect.left() < currentSceneRect.left() + margin) {
        newLeft = viewRect.left() - margin * 2;
        needsExpansion = true;
    }
    if (viewRect.top() < currentSceneRect.top() + margin) {
        newTop = viewRect.top() - margin * 2;
        needsExpansion = true;
    }
    if (viewRect.right() > currentSceneRect.right() - margin) {
        newRight = viewRect.right() + margin * 2;
        needsExpansion = true;
    }
    if (viewRect.bottom() > currentSceneRect.bottom() - margin) {
        newBottom = viewRect.bottom() + margin * 2;
        needsExpansion = true;
    }

    if (needsExpansion) {
        QRectF newSceneRect(newLeft, newTop,
                           newRight - newLeft,
                           newBottom - newTop);
        scene_->setSceneRect(newSceneRect);
    }
}

void ASTGraphView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor); // Restore default cursor
    }
    QGraphicsView::mouseReleaseEvent(event);
}

// ---------- helpers ----------
QString ASTGraphView::nodeIdentifier(const QJsonObject &node) const {
    if (!node.contains("children")) return QString();
    QJsonArray children = node["children"].toArray();
    for (const auto &v : children) {
        QJsonObject ch = v.toObject();
        if (ch["type"].toString() == "identifier" && ch.contains("text"))
            return ch["text"].toString();
    }
    return QString();
}

QString ASTGraphView::enclosingClassName(const QJsonObject &node) const {
    if (!rootAst_.contains("children")) return QString();
    QJsonArray rootChildren = rootAst_["children"].toArray();

    int nodeStart = node["start_row"].toInt();
    int nodeEnd   = node["end_row"].toInt();

    for (const auto &cVal : rootChildren) {
        QJsonObject c = cVal.toObject();
        QString type = c["type"].toString();
        if (!type.contains("class") && !type.contains("struct") && !type.contains("namespace"))
            continue;

        int cStart = c["start_row"].toInt();
        int cEnd   = c["end_row"].toInt();

        // node lies entirely inside this class/struct/namespace
        if (nodeStart >= cStart && nodeEnd <= cEnd) {
            QString name = nodeIdentifier(c);
            if (!name.isEmpty())
                return name;
        }
    }
    return QString();
}

// ------------------------------------------------------------------
// Recursive identifier lookup
QString ASTGraphView::findIdentifier(const QJsonObject &node) const {
    if (!node.contains("children")) return QString();

    for (const auto &childVal : node["children"].toArray()) {
        QJsonObject child = childVal.toObject();
        if (child["type"].toString() == "identifier" && child.contains("text"))
            return child["text"].toString();

        QString res = findIdentifier(child);
        if (!res.isEmpty())
            return res;
    }
    return QString();
}

// ------------------------------------------------------------------
// Recursively find the name of the outermost class/struct/namespace enclosing the node
QString ASTGraphView::findEnclosingClassName(const QJsonObject &node) const {
    if (!rootAst_.contains("children")) return QString();

    std::function<QString(const QJsonObject&, int, int)> dfs;
    dfs = [&](const QJsonObject &n, int minRow, int maxRow) -> QString {
        int start = n["start_row"].toInt();
        int end   = n["end_row"].toInt();

        if (start < minRow || end > maxRow) return QString(); // out of range

        QString type = n["type"].toString();
        if (type.contains("class") || type.contains("struct") || type.contains("namespace")) {
            QString name = findIdentifier(n);
            if (!name.isEmpty())
                return name;
        }

        if (n.contains("children")) {
            for (const auto &cVal : n["children"].toArray()) {
                QString res = dfs(cVal.toObject(), minRow, maxRow);
                if (!res.isEmpty())
                    return res;
            }
        }
        return QString();
    };

    return dfs(rootAst_, node["start_row"].toInt(), node["end_row"].toInt());
}

// ------------------------------------------------------------------
// Display label for a node
QString ASTGraphView::nodeLabel(const QJsonObject &node) const {
    QString type = node["type"].toString();

    // Function
    if (type.contains("function")) {
        QString funcName = findIdentifier(node);
        if (!funcName.isEmpty()) {
            QString className = findEnclosingClassName(node);
            if (!className.isEmpty())
                return className + "::" + funcName;
            return funcName;
        }
    }

    // Class/struct/namespace
    if (type.contains("class") || type.contains("struct") || type.contains("namespace")) {
        QString name = findIdentifier(node);
        if (!name.isEmpty())
            return name;
    }

    // Anything else: the node type
    return type;
}

// ---------- double click ----------
void ASTGraphView::mouseDoubleClickEvent(QMouseEvent *event) {
    QPointF p = mapToScene(event->pos());
    for (const auto &n : nodes) {
        if (n.rect.contains(p)) {
            // notify about the double click (opens the editor)
            emit nodeDoubleClicked(nodeMap_[n.id]);

            // jump to the row/column, as with a single click
            emit openAt(n.start_row, n.start_col);

            break;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void ASTGraphView::wheelEvent(QWheelEvent *event)
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

    qreal steps = delta / 120.0; // 120 == one mouse wheel step
    qreal factor = qPow(m_zoomStep, steps);
    qreal targetScale = qBound(m_minScale, m_totalScale * factor, m_maxScale);
    qreal actualFactor = targetScale / m_totalScale;

    if (qFuzzyCompare(actualFactor, 1.0)) {
        event->accept();
        return;
    }

    // Perform zoom centered on cursor position
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scale(actualFactor, actualFactor);
    setTransformationAnchor(QGraphicsView::NoAnchor); // Reset to default behavior

    m_totalScale = targetScale;

    // Expand scene if needed after zooming
    expandSceneIfNeeded();
    event->accept();
}

void ASTGraphView::resetZoom()
{
    if (!scene_ || scene_->items().isEmpty()) {
        resetTransform();
        m_totalScale = 1.0;
        return;
    }

    fitInView(scene_->itemsBoundingRect(), Qt::KeepAspectRatio);
    m_totalScale = transform().m11();
}
