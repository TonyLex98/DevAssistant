#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

class ASTGraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ASTGraphView(QWidget *parent = nullptr);
    void setAST(const QJsonObject &ast);

signals:
    void openAt(int line, int column);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QGraphicsScene *scene_;
    struct Node { int id; QRectF rect; int start_row=0; int start_col=0; };
    QVector<Node> nodes;

    QJsonObject rootAst_;                          // ← хранит весь JSON‑объект

    // Вспомогательные функции для формирования надписей
    QString nodeLabel(const QJsonObject &node) const;
    QString nodeIdentifier(const QJsonObject &node) const;
    QString enclosingClassName(const QJsonObject &node) const;


    QString findIdentifier(const QJsonObject &node) const;
    QString findEnclosingClassName(const QJsonObject &node) const;
};

