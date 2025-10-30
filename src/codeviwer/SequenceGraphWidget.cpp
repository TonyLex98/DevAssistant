#include "SequenceGraphWidget.h"
#include <QVBoxLayout>

SequenceGraphWidget::SequenceGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Sequence Relationships Graph");
    setMinimumSize(800, 600);

    m_view = new SequenceRelationView(this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    setLayout(layout);

    connect(m_view, &SequenceRelationView::sequenceSelected, this, [this](const QString &sequenceName) {
        // This will be connected to the main window to open the sequence editor
        emit sequenceSelected(sequenceName);
    });
}

void SequenceGraphWidget::setSequenceManager(PromptSequenceManager *seqMgr)
{
    m_view->setSequenceManager(seqMgr);
}

void SequenceGraphWidget::setCurrentSequence(const QString &sequenceName)
{
    m_view->setCurrentSequence(sequenceName);
}

void SequenceGraphWidget::rebuildGraph()
{
    m_view->rebuildGraph();
}