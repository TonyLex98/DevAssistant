#include "astgraphview.h"
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QMouseEvent>

ASTGraphView::ASTGraphView(QWidget *parent) : QGraphicsView(parent) {
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
}

void ASTGraphView::setAST(const QJsonObject &ast) {
    scene_->clear();
    nodes.clear();

    // Simple layered layout: collect top-level functions/classes
    QJsonArray children = ast["children"].toArray();
    int x = 20, y = 20;
    const int w = 220, h = 44, dx = 260, dy = 70;

    int id = 0;
    for (const auto &childVal : children) {
        QJsonObject c = childVal.toObject();
        QString type = c["type"].toString();
        if (type.contains("function") || type.contains("class") || type.contains("struct") || type.contains("namespace")) {
            QString label = type;
            if (c.contains("children")) {
                auto grand = c["children"].toArray();
                for (const auto &gv : grand) {
                    QJsonObject go = gv.toObject();
                    if (go["type"].toString().contains("identifier") && go.contains("text")) {
                        label += " " + go["text"].toString();
                        break;
                    }
                }
            }

            auto *rect = scene_->addRect(x, y, w, h, QPen(Qt::black), QBrush(QColor(230, 244, 255)));
            auto *txt = scene_->addText(label);
            txt->setPos(x+8, y+8);

            Node n; n.id = id++; n.rect = QRectF(x,y,w,h);
            n.start_row = c["start_row"].toInt();
            n.start_col = c["start_column"].toInt();
            nodes.push_back(n);

            // connect with a line from root (optional): skipped for clarity

            x += dx;
            if (x > 1000) { x = 20; y += dy; }
        }
    }
    scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-50,-50,50,50));
}

void ASTGraphView::mousePressEvent(QMouseEvent *event) {
    QPointF p = mapToScene(event->pos());
    for (const auto &n : nodes) {
        if (n.rect.contains(p)) {
            emit openAt(n.start_row, n.start_col);
            break;
        }
    }
    QGraphicsView::mousePressEvent(event);
}
