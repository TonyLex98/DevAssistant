#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QSplitter>
#include <QLabel>
#include <QListWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGridLayout>
#include "SequenceRelationView.h"
#include "SolutionTemplate.h"
#include "SolutionTemplateManager.h"
#include "PromptSequenceManager.h"
#include "../ui/customWidgets/StyledButton.h"
#include "../ui/customWidgets/MemoryLineEdit.h"

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
    QTabWidget *m_tabs;
    
    // Template Info Tab
    QWidget *m_templateInfoTab;
    MemoryLineEdit *m_nameEdit;
    MemoryLineEdit *m_descriptionEdit;
    QLabel *m_imageLabel;
    StyledButton *m_selectImageBtn;
    
    // Sequences Tab
    QWidget *m_sequencesTab;
    QHBoxLayout *m_sequencesLayout;
    QVBoxLayout *m_availableSequencesLayout;
    QListWidget *m_availableSequencesList;
    QVBoxLayout *m_templateSequencesLayout;
    QListWidget *m_templateSequencesList;
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