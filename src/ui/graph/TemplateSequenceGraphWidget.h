#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGridLayout>
#include "ui/graph/SequenceRelationView.h"
#include "core/workflow/SolutionTemplate.h"
#include "core/workflow/SolutionTemplateManager.h"
#include "core/workflow/PromptSequenceManager.h"
#include "ui/widgets/StyledButton.h"
#include "ui/widgets/MemoryLineEdit.h"
#include "ui/widgets/StyledTabWidget.h"
#include "ui/widgets/StyledLabel.h"
#include "ui/widgets/StyledListWidget.h"

class TemplateSequenceGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TemplateSequenceGraphWidget(QWidget *parent = nullptr);
    
    void setTemplateManager(SolutionTemplateManager *templateManager);
    void setSequenceManager(PromptSequenceManager *seqManager);
    void setTemplate(const SolutionTemplate &templateValue);

public slots:
    void refreshGraph();

signals:
    void sequenceSelected(const QString &sequenceName);
    void templateModified();

private slots:
    void onAddSequenceToTemplate();
    void onRemoveSequenceFromTemplate();
    void onSetMainSequence();
    void onSequenceInTemplateSelected();
    void onExternalSequenceSelected();
    void onGraphSequenceSelected(const QString &sequenceName);
    void onSelectImage();

private:
    void setupUI();
    void updateAvailableSequences();
    void updateTemplateSequencesList();
    void buildGraphForTemplate();
    
    // UI Components
    QVBoxLayout *m_mainLayout;
    StyledTabWidget *m_tabs;
    
    // Template Info Tab
    QWidget *m_templateInfoTab;
    MemoryLineEdit *m_nameEdit;
    MemoryLineEdit *m_descriptionEdit;
    StyledLabel *m_imageLabel;
    StyledButton *m_selectImageBtn;
    
    // Sequences Tab
    QWidget *m_sequencesTab;
    QHBoxLayout *m_sequencesLayout;
    QVBoxLayout *m_availableSequencesLayout;
    StyledListWidget *m_availableSequencesList;
    QVBoxLayout *m_templateSequencesLayout;
    StyledListWidget *m_templateSequencesList;
    QVBoxLayout *m_buttonsLayout;
    StyledButton *m_addSequenceBtn;
    StyledButton *m_removeSequenceBtn;
    StyledButton *m_setMainSequenceBtn;
    
    // Graph Tab
    QWidget *m_graphTab;
    QVBoxLayout *m_graphLayout;
    SequenceRelationView *m_graphView;
    StyledButton *m_refreshGraphBtn;
    
    // Managers and data
    SolutionTemplateManager *m_templateManager = nullptr;
    PromptSequenceManager *m_sequenceManager = nullptr;
    SolutionTemplate m_currentTemplate;
    QString m_imagePath;
};