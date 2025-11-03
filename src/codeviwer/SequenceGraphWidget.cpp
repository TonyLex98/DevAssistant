#include "SequenceGraphWidget.h"
#include <QVBoxLayout>
#include "../core/SolutionTemplateManager.h"

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
    
    qDebug() << "[SequenceGraphWidget] Constructor completed";
}

void SequenceGraphWidget::setSequenceManager(PromptSequenceManager *seqMgr)
{
    qDebug() << "[SequenceGraphWidget] setSequenceManager called";
    m_view->setSequenceManager(seqMgr);
}

void SequenceGraphWidget::setTemplateManager(SolutionTemplateManager *templateMgr)
{
    Q_UNUSED(templateMgr); // For now, we store this in the view
    qDebug() << "[SequenceGraphWidget] setTemplateManager called";
    if (m_view) {
        // We don't store it directly in the widget, but the view can access templates as needed
        // The view doesn't currently store the template manager, but may need to in the future
    }
}

void SequenceGraphWidget::setCurrentSequence(const QString &sequenceName)
{
    m_view->setCurrentSequence(sequenceName);
}

void SequenceGraphWidget::setTemplateFilter(const QString &templateDirPath)
{
    qDebug() << "[SequenceGraphWidget] setTemplateFilter called with:" << templateDirPath;
    // In a full implementation, we would use this to filter
    m_view->rebuildGraphForTemplate(templateDirPath);
}

void SequenceGraphWidget::rebuildGraph()
{
    qDebug() << "[SequenceGraphWidget] rebuildGraph called";
    m_view->rebuildGraph();
}

void SequenceGraphWidget::rebuildGraphForTemplate(const QString &templateDirPath)
{
    qDebug() << "[SequenceGraphWidget] rebuildGraphForTemplate called with:" << templateDirPath;
    m_view->rebuildGraphForTemplate(templateDirPath);
}

void SequenceGraphWidget::saveLayout()
{
    qDebug() << "[SequenceGraphWidget] saveLayout called";
    m_view->saveLayout();
}

void SequenceGraphWidget::restoreLayout()
{
    qDebug() << "[SequenceGraphWidget] restoreLayout called";
    m_view->restoreLayout();
}