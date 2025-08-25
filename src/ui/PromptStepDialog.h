// PromptStepDialog.h
#pragma once
#include <QDialog>
#include "PromptStep.h"
#include <QTextEdit>
#include <QDialogButtonBox>

class QTextEdit;
class QListWidget;
class QPushButton;

class PromptStepDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PromptStepDialog(QWidget *parent = nullptr);
    void setStep(const PromptStep& step);
    PromptStep step() const { return m_step; }

private slots:
    void onOk();

private:
    QTextEdit   *m_templateEdit = nullptr;
    QListWidget *m_cmdList      = nullptr;
    PromptStep   m_step;
};
