#include "ui/graph/SequenceGraphLayout.h"
#include <QtGlobal>

namespace {
constexpr qreal kColumnGap = 48.0;       // vertical space between boxes, room for an arrow
constexpr qreal kScenePadding = 100.0;
constexpr qreal kMinSceneSide = 1000.0;
constexpr qreal kPanMargin = 200.0;      // how close to the border the view may get before the scene grows
} // namespace

namespace SequenceGraphLayout {

QVector<QPointF> columnPositions(const QVector<qreal> &nodeHeights)
{
    QVector<QPointF> positions;
    positions.reserve(nodeHeights.size());
    qreal y = 0;
    for (qreal height : nodeHeights) {
        positions.append(QPointF(0, y + height / 2));
        y += height + kColumnGap;
    }
    return positions;
}

QRectF sceneRectAround(const QRectF &nodesBounds)
{
    if (nodesBounds.isEmpty())
        return QRectF(-kMinSceneSide / 2, -kMinSceneSide / 2, kMinSceneSide, kMinSceneSide);

    const QRectF padded = nodesBounds.adjusted(-kScenePadding, -kScenePadding, kScenePadding, kScenePadding);
    const qreal width = qMax(padded.width(), kMinSceneSide);
    const qreal height = qMax(padded.height(), kMinSceneSide);
    // Keep the nodes in the middle when the minimum size adds space.
    const qreal left = padded.left() - (width - padded.width()) / 2;
    const qreal top = padded.top() - (height - padded.height()) / 2;
    return QRectF(left, top, width, height);
}

bool expandedSceneRect(const QRectF &sceneRect, const QRectF &visibleRect, QRectF *expanded)
{
    qreal newLeft = sceneRect.left();
    qreal newTop = sceneRect.top();
    qreal newRight = sceneRect.right();
    qreal newBottom = sceneRect.bottom();
    bool needsExpansion = false;

    // Growing by twice the margin means the next few pan steps do not trigger another resize.
    if (visibleRect.left() < sceneRect.left() + kPanMargin) {
        newLeft = visibleRect.left() - kPanMargin * 2;
        needsExpansion = true;
    }
    if (visibleRect.top() < sceneRect.top() + kPanMargin) {
        newTop = visibleRect.top() - kPanMargin * 2;
        needsExpansion = true;
    }
    if (visibleRect.right() > sceneRect.right() - kPanMargin) {
        newRight = visibleRect.right() + kPanMargin * 2;
        needsExpansion = true;
    }
    if (visibleRect.bottom() > sceneRect.bottom() - kPanMargin) {
        newBottom = visibleRect.bottom() + kPanMargin * 2;
        needsExpansion = true;
    }

    if (!needsExpansion)
        return false;

    *expanded = QRectF(newLeft, newTop, newRight - newLeft, newBottom - newTop);
    return true;
}

qreal readableScale(qreal fittedScale)
{
    return qBound<qreal>(0.5, fittedScale, 1.0);
}

} // namespace SequenceGraphLayout
