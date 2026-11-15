#pragma once
#include <QWidget>
#include "PromptStep.h"
#include <StyleTextEdit.h>
#include "PromptSequenceManager.h"  // Include the header for sequence manager
#include <StyledButton.h>
#include <QCheckBox>
#include <QListWidget>  // Include the header that also defines QListWidgetItem
#include "customWidgets/MemoryLineEdit.h"

class StyleTextEdit;
class StyledButton;

class PromptStepDialog : public QWidget
{
    Q_OBJECT
public:
    explicit PromptStepDialog(QWidget *parent = nullptr);
    void setStep(const PromptStep& step);
    void setSequenceManager(PromptSequenceManager* seqMgr);  // Add sequence manager
    void setCurrentSequencePath(const QString& sequencePath); // Add path of current sequence for filtering
    void setCurrentSequenceName(const QString& sequenceName); // Set name of current sequence being edited
    PromptStep step() const;
    void focusTemplate();

signals:
    void stepEdited(const PromptStep& step);



private slots:
    void onInsertClicked();
    void onAddSequenceInfo();  // New slot for adding sequence info resolver
    void loadAvailableSequences(); // Load and display available sequences
    void onSequenceItemChanged(QListWidgetItem *item); // Handle sequence item state changes

private:
    PromptStep buildStepFromUi() const;
    void emitEdited();
    StyleTextEdit   *m_templateEdit = nullptr;
    QListWidget *m_cmdList      = nullptr;
    MemoryLineEdit *m_cmdFilter = nullptr;
    PromptStep   m_step;
    PromptSequenceManager *m_seqMgr = nullptr;  // Reference to sequence manager
    QString m_templateDir; // Directory of the current template for filtering
    QString m_currentSequenceName; // Name of the current sequence being edited

    /* --- Resolver insertion --- */
    QListWidget *m_insertList = nullptr;
    MemoryLineEdit *m_insertFilter = nullptr;
    StyledButton *m_insertBtn      = nullptr;

    /* --- Loop option --- */
    QCheckBox   *m_loopCheck      = nullptr;
    
    /* --- Summarization option --- */
    QCheckBox   *m_summarizeCheck = nullptr;
    
    /* --- Sequence info resolver --- */
    StyledButton *m_addSequenceInfoBtn = nullptr;
    QListWidget *m_sequenceListWidget = nullptr;
    MemoryLineEdit *m_seqFilter = nullptr;
};
