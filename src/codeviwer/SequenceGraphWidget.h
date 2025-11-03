#pragma once

#include <QWidget>
#include "SequenceRelationView.h"
#include <QString>

class SolutionTemplateManager;

class SequenceGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SequenceGraphWidget(QWidget *parent = nullptr);
    
    void setSequenceManager(PromptSequenceManager *seqMgr);
    void setTemplateManager(SolutionTemplateManager *templateMgr);  // Set the template manager
    void setCurrentSequence(const QString &sequenceName);  // Highlight the current sequence
    void setTemplateFilter(const QString &templateDirPath); // Filter sequences by template

public slots:
    void rebuildGraph();
    void rebuildGraphForTemplate(const QString &templateDirPath); // Rebuild for specific template
    void saveLayout();     // Save the current layout
    void restoreLayout();  // Restore the saved layout

signals:
    void sequenceSelected(const QString &sequenceName);

private:
    SequenceRelationView *m_view{nullptr};
};