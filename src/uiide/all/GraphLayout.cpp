#include "GraphLayout.h"
#include "GraphItems.h"
#include <QGraphicsScene>
#include <QtMath>
#include <QRandomGenerator>
#include <QAbstractAnimation>

GraphLayout::GraphLayout(QObject *parent): QObject(parent){}

void GraphLayout::applyAutoLayout(QGraphicsScene* scene, const QList<ClassItem*>& classes){
    if(classes.isEmpty()) return;
    int cols = qCeil(qSqrt((qreal)classes.size()));
    QPointF start(0,0); qreal sx=480, sy=320; int i=0;
    for(auto *c: classes){
        int row = i/cols, col = i%cols;
        QPointF target = start + QPointF(col*sx, row*sy);
        auto *anim = new QVariantAnimation(scene);
        anim->setDuration(600+QRandomGenerator::global()->bounded(300));
        anim->setStartValue(c->pos());
        anim->setEndValue(target + QPointF(QRandomGenerator::global()->bounded(30),QRandomGenerator::global()->bounded(30)));
        QObject::connect(anim,&QVariantAnimation::valueChanged, c, [c](const QVariant &v){ c->setPos(v.toPointF()); });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        ++i;
    }
}
