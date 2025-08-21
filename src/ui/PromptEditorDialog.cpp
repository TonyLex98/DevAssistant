#include "PromptEditorDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>

PromptEditorDialog::PromptEditorDialog(PromptManager* mgr, QWidget* parent):QDialog(parent),m_mgr(mgr){
    setWindowTitle("Base Prompt & History");
    resize(720,520);
    auto v = new QVBoxLayout(this);
    m_text = new QPlainTextEdit(this);
    m_text->setPlainText(m_mgr->current_all_prompt);
    v->addWidget(new QLabel("Base prompt text (stored on disk):", this));
    v->addWidget(m_text,1);
    auto f = new QFormLayout();
    m_hist = new QSpinBox(this); m_hist->setRange(0, 50); m_hist->setValue(m_mgr->historySize());
    f->addRow("History turns (N):", m_hist);
    v->addLayout(f);
    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Close, this);
    v->addWidget(bb);
    connect(bb->button(QDialogButtonBox::Save), &QPushButton::clicked, this, &PromptEditorDialog::onSave);
    connect(bb->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &PromptEditorDialog::accept);
}

void PromptEditorDialog::onSave(){
    //m_mgr->saveBasePrompt(m_text->toPlainText());
    //m_mgr->setHistorySize(m_hist->value());
    accept();
}
