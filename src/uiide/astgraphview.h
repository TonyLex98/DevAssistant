#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QtMath>
#include <QScrollBar>

class ASTGraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ASTGraphView(QWidget *parent = nullptr);
    void setAST(const QJsonObject &ast);

signals:
    void openAt(int line, int column);
    void nodeDoubleClicked(const QJsonObject &node);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QGraphicsScene *scene_;
    struct Node { int id; QRectF rect; int start_row=0; int start_col=0; };
    QVector<Node> nodes;

    QJsonObject rootAst_;                          // whole AST JSON object
    QMap<int, QJsonObject> nodeMap_;    // node id -> JSON node

    // Helpers for node labels
    QString nodeLabel(const QJsonObject &node) const;
    QString nodeIdentifier(const QJsonObject &node) const;
    QString enclosingClassName(const QJsonObject &node) const;


    QString findIdentifier(const QJsonObject &node) const;
    QString findEnclosingClassName(const QJsonObject &node) const;

    // Zoom functionality
    void wheelEvent(QWheelEvent *event) override;
    void resetZoom();

private:
    // Variables for panning functionality
    bool m_dragging = false;
    QPoint m_lastPanPoint;

    // Variables for zoom functionality
    qreal m_totalScale = 1.0;
    qreal m_minScale = 0.1;
    qreal m_maxScale = 10.0;
    qreal m_zoomStep = 1.15;

    // Function to expand scene when panning near edges
    void expandSceneIfNeeded();
};

