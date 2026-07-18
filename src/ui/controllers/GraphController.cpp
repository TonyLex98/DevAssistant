#include "GraphController.h"

GraphController::GraphController(IGraphService* graph)
    : QObject(nullptr)
    , m_graph(graph)
{
}

void GraphController::openGraph()
{
    m_graph->showGraphWindow();
}

void GraphController::openLightRag()
{
    m_graph->showLightRagWindow();
}
