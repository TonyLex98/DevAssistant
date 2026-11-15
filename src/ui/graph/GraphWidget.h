#pragma once

#include <QWidget>
#include "ui/widgets/StyledButton.h"
#include "ui/graph/ProjectRelationView.h"

class GraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GraphWidget(QWidget *parent = nullptr);

    ProjectRelationView *view() const { return m_view; }

private:
    ProjectRelationView *m_view{nullptr};
    StyledButton        *m_openBtn{nullptr};
};
