#pragma once

#include <QWidget>
#include <QPushButton>
#include "ProjectRelationView.h"

class GraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GraphWidget(QWidget *parent = nullptr);

    ProjectRelationView *view() const { return m_view; }

private:
    ProjectRelationView *m_view{nullptr};
    QPushButton        *m_openBtn{nullptr};
};
