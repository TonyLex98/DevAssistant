#pragma once
#include <QObject>
#include "../../services/IService.h"

class GraphController : public QObject {
    Q_OBJECT
public:
    explicit GraphController(IGraphService* graph);

public slots:
    void openGraph();
    void openLightRag();

private:
    IGraphService* m_graph;
};
