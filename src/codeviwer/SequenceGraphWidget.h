#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "SequenceRelationView.h"
#include <QString>
#include "../ui/customWidgets/MemoryComboBox.h"
#include <StyledButton.h>

class SolutionTemplateManager;
class PromptSequenceManager;  // Forward declaration instead of including header to avoid circular includes

class SequenceGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SequenceGraphWidget(QWidget *parent = nullptr);
    
    void setSequenceManager(PromptSequenceManager *seqMgr);
    void setTemplateManager(SolutionTemplateManager *templateMgr);  // Set the template manager
    void setCurrentSequence(const QString &sequenceName);  // Highlight the current sequence
    void setTemplateFilter(const QString &templateDirPath); // Filter sequences by template
    void populateTemplateComboBox(); // Populate the combobox with available templates

public slots:
    void rebuildGraph();
    void rebuildGraphForTemplate(const QString &templateDirPath); // Rebuild for specific template
    void saveLayout();     // Save the current layout
    void restoreLayout();  // Restore the saved layout
    void onTemplateComboBoxChanged(); // Handle template selection change
    void setTemplateSelectionByPath(const QString &templatePath);
    QString currentTemplatePath() const;

signals:
    void sequenceSelected(const QString &sequenceName);

private:
    SequenceRelationView *m_view{nullptr};
    PromptSequenceManager *m_seqMgr{nullptr}; // reference to sequence manager
    SolutionTemplateManager *m_templateMgr{nullptr}; // reference to template manager for populating combobox
    MemoryComboBox *m_templateComboBox{nullptr};
    QLabel *m_templateLabel{nullptr};
    QVBoxLayout *m_mainLayout{nullptr};
    QString m_currentTemplatePath;
};
