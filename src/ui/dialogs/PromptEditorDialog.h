#pragma once
#include <QDialog>
#include <QPlainTextEdit>
#include <QSpinBox>
#include "core/context/PromptManager.h"

class PromptEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit PromptEditorDialog(PromptManager* mgr, QWidget* parent=nullptr);

private slots:
    void onSave();

private:
    PromptManager* m_mgr=nullptr;
    QPlainTextEdit* m_text=nullptr;
    QSpinBox* m_hist=nullptr;
};
