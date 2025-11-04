#pragma once
#include <QDialog>
#include "PromptStep.h"
#include <StyleTextEdit.h>
#include <QDialogButtonBox>
#include "PromptSequenceManager.h"  // Include the header for sequence manager
#include <StyledButton.h>
#include "customWidgets/MemoryComboBox.h"
#include <QCheckBox>

class StyleTextEdit;
class QListWidget;
class StyledButton;

class PromptStepDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PromptStepDialog(QWidget *parent = nullptr);
    void setStep(const PromptStep& step);
    void setSequenceManager(PromptSequenceManager* seqMgr);  // Add sequence manager
    void setCurrentSequencePath(const QString& sequencePath); // Add path of current sequence for filtering
    PromptStep step() const { return m_step; }



private slots:
    void onOk();
    void onInsertClicked();    // слот «Вставить»
    void onAddSequenceLink();  // New slot for adding sequence links

private:
    StyleTextEdit   *m_templateEdit = nullptr;
    QListWidget *m_cmdList      = nullptr;
    PromptStep   m_step;
    PromptSequenceManager *m_seqMgr = nullptr;  // Reference to sequence manager
    QString m_templateDir; // Directory of the current template for filtering

    /* --- UI для резолверов --- */
    MemoryComboBox   *m_resolverCombo = nullptr;
    StyledButton *m_insertBtn      = nullptr;

    /* --- UI для зацикливания --- */
    QCheckBox   *m_loopCheck      = nullptr;
    
    /* --- UI для саммаризации --- */
    QCheckBox   *m_summarizeCheck = nullptr;
    
    /* --- UI для ссылок на последовательности --- */
    StyledButton *m_addSequenceLinkBtn = nullptr;
};
