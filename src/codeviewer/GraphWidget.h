#pragma once

#include <QWidget>
#include "../ui/customWidgets/StyledButton.h"
#include "ProjectRelationView.h"

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
