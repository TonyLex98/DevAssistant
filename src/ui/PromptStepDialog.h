#pragma once
#include <QDialog>
#include "PromptStep.h"
#include <StyleTextEdit.h>
#include <QDialogButtonBox>
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
    PromptStep step() const { return m_step; }



private slots:
    void onOk();
    void onInsertClicked();    // слот «Вставить»

private:
    StyleTextEdit   *m_templateEdit = nullptr;
    QListWidget *m_cmdList      = nullptr;
    PromptStep   m_step;

    /* --- UI для резолверов --- */
    QComboBox   *m_resolverCombo = nullptr;
    StyledButton *m_insertBtn      = nullptr;

    /* --- UI для зацикливания --- */
    QCheckBox   *m_loopCheck      = nullptr;
};
