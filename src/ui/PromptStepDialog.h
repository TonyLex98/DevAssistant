#pragma once
#include <QDialog>
#include "PromptStep.h"
#include <StyleTextEdit.h>
#include <QDialogButtonBox>
#include "PromptSequenceManager.h"  // Include the header for sequence manager
#include <StyledButton.h>
#include <QComboBox>
#include <QCheckBox>

class StyleTextEdit;
class QListWidget;
class StyledButton;
class QComboBox;

class PromptStepDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PromptStepDialog(QWidget *parent = nullptr);
    void setStep(const PromptStep& step);
    void setSequenceManager(PromptSequenceManager* seqMgr);  // Add sequence manager
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

    /* --- UI для резолверов --- */
    QComboBox   *m_resolverCombo = nullptr;
    StyledButton *m_insertBtn      = nullptr;

    /* --- UI для зацикливания --- */
    QCheckBox   *m_loopCheck      = nullptr;
    
    /* --- UI для саммаризации --- */
    QCheckBox   *m_summarizeCheck = nullptr;
    
    /* --- UI для ссылок на последовательности --- */
    StyledButton *m_addSequenceLinkBtn = nullptr;
};
