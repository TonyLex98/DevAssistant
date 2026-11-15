#include "ui/graph/GraphWidget.h"
#include <QVBoxLayout>
#include <QFileDialog>

GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(tr("Project Graph"));
    setMinimumSize(600, 400);

    m_openBtn = new StyledButton(tr("Open Project…"), this);
    m_view    = new ProjectRelationView(this);
    //m_view->hide();

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_openBtn);
    layout->addWidget(m_view);
    setLayout(layout);

    connect(m_openBtn, &StyledButton::clicked, this, [this](){
        QString root = QFileDialog::getExistingDirectory(
            this, tr("Select project folder"), QDir::homePath());
        if (root.isEmpty()) return;

        m_view->setProjectRoot(root);   // starts building the graph
        m_view->show();
    });
}
