#pragma once
#include <QPointF>
#include <QRectF>
#include <QVector>

// Geometry of the sequence graph that needs no view: the default node placement,
// the scene size, and how far an automatic fit may zoom.
namespace SequenceGraphLayout {

// Space left around the nodes when the whole graph or the selection is fitted into the view.
constexpr qreal kFitMargin = 80.0;
constexpr qreal kFocusMargin = 60.0;

// Centres of nodes stacked top to bottom in one column, for node heights given in the same order.
// One column fits the tall side panel, and nodes of any width never overlap
// (on a circle, wide nodes ran into each other).
QVector<QPointF> columnPositions(const QVector<qreal> &nodeHeights);

// Scene rectangle around the nodes, with room to pan even when the graph is small.
QRectF sceneRectAround(const QRectF &nodesBounds);

// Grows the scene when the visible area comes close to its border, so panning never hits a wall.
// Returns false and leaves *expanded untouched when the scene is already large enough.
bool expandedSceneRect(const QRectF &sceneRect, const QRectF &visibleRect, QRectF *expanded);

// Clamps the scale of an automatic fit: below 50% the names become unreadable,
// above 100% a small graph would be blown up.
qreal readableScale(qreal fittedScale);

} // namespace SequenceGraphLayout
