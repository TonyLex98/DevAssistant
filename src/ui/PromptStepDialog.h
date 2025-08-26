#pragma once
#include <QDialog>
#include "PromptStep.h"
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QComboBox>          // ← новый заголовок

class QTextEdit;
class QListWidget;
class QPushButton;
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
    QTextEdit   *m_templateEdit = nullptr;
    QListWidget *m_cmdList      = nullptr;
    PromptStep   m_step;

    /* --- UI для резолверов --- */
    QComboBox   *m_resolverCombo = nullptr;
    QPushButton *m_insertBtn      = nullptr;
};
