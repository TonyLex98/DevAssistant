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

/*
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
*/

// Основная логика построения графа
// ---------- построение графа ----------
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

            nodeMap_[n.id] = c;    // <‑‑ храним объект

            x += dx;
            if (x > 1000) { x = 20; y += dy; }
        }
    }

    scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-50, -50, 50, 50));
}

// ------------------------------------------------------------------
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

// ---------- Вспомогательные функции ----------
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

        // если узел полностью находится внутри класса/структуры/namespace
        if (nodeStart >= cStart && nodeEnd <= cEnd) {
            QString name = nodeIdentifier(c);
            if (!name.isEmpty())
                return name;
        }
    }
    return QString();
}

// ------------------------------------------------------------------
// Рекурсивный поиск идентификатора
QString ASTGraphView::findIdentifier(const QJsonObject &node) const {
    if (!node.contains("children")) return QString();

    for (const auto &childVal : node["children"].toArray()) {
        QJsonObject child = childVal.toObject();
        if (child["type"].toString() == "identifier" && child.contains("text"))
            return child["text"].toString();

        // Ищем дальше рекурсивно
        QString res = findIdentifier(child);
        if (!res.isEmpty())
            return res;
    }
    return QString();
}

// ------------------------------------------------------------------
// Рекурсивный поиск имени самого верхнего (или ближайшего) класса/struct/namespace
QString ASTGraphView::findEnclosingClassName(const QJsonObject &node) const {
    if (!rootAst_.contains("children")) return QString();

    std::function<QString(const QJsonObject&, int, int)> dfs;
    dfs = [&](const QJsonObject &n, int minRow, int maxRow) -> QString {
        int start = n["start_row"].toInt();
        int end   = n["end_row"].toInt();

        if (start < minRow || end > maxRow) return QString(); // не в диапазоне

        QString type = n["type"].toString();
        if (type.contains("class") || type.contains("struct") || type.contains("namespace")) {
            QString name = findIdentifier(n);
            if (!name.isEmpty())
                return name;
        }

        // Переходим к потомкам
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
// Формируем надпись для узла
QString ASTGraphView::nodeLabel(const QJsonObject &node) const {
    QString type = node["type"].toString();

    // Функция
    if (type.contains("function")) {
        QString funcName = findIdentifier(node);
        if (!funcName.isEmpty()) {
            QString className = findEnclosingClassName(node);
            if (!className.isEmpty())
                return className + "::" + funcName;
            return funcName;
        }
    }

    // Класс/структура/namespace
    if (type.contains("class") || type.contains("struct") || type.contains("namespace")) {
        QString name = findIdentifier(node);
        if (!name.isEmpty())
            return name;
    }

    // Для остальных типов – просто имя типа
    return type;
}

// ---------- двойной щелчок ----------
void ASTGraphView::mouseDoubleClickEvent(QMouseEvent *event) {
    QPointF p = mapToScene(event->pos());
    for (const auto &n : nodes) {
        if (n.rect.contains(p)) {
            emit nodeDoubleClicked(nodeMap_[n.id]);   // посылаем целый JSON‑узел
            break;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

