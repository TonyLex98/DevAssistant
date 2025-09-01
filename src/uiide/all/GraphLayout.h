#pragma once
#include <QObject>
#include <QVariantAnimation>
#include <QPointF>
#include <QList>

class QGraphicsScene;
class ClassItem;

class GraphLayout : public QObject {
    Q_OBJECT
public:
    explicit GraphLayout(QObject *parent=nullptr);
    void applyAutoLayout(QGraphicsScene* scene, const QList<ClassItem*>& classes);
};
