// PromptSequenceEditor.cpp
#include "PromptSequenceEditor.h"
#include "PromptStepDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>

PromptSequenceEditor::PromptSequenceEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *v = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    v->addWidget(m_list, 1);

    // Кнопки управления
    auto *h = new QHBoxLayout();
    m_addBtn   = new QPushButton("Add", this);
    m_editBtn  = new QPushButton("Edit", this);
    m_delBtn   = new QPushButton("Delete", this);
    m_upBtn    = new QPushButton("Up", this);
    m_downBtn  = new QPushButton("Down", this);
    h->addWidget(m_addBtn);
    h->addWidget(m_editBtn);
    h->addWidget(m_delBtn);
    h->addWidget(m_upBtn);
    h->addWidget(m_downBtn);
    v->addLayout(h);

    connect(m_addBtn,   &QPushButton::clicked, this, &PromptSequenceEditor::onAddStep);
    connect(m_editBtn,  &QPushButton::clicked, this, &PromptSequenceEditor::onEditStep);
    connect(m_delBtn,   &QPushButton::clicked, this, &PromptSequenceEditor::onRemoveStep);
    connect(m_upBtn,    &QPushButton::clicked, this, &PromptSequenceEditor::onMoveUp);
    connect(m_downBtn,  &QPushButton::clicked, this, &PromptSequenceEditor::onMoveDown);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &PromptSequenceEditor::onEditStep);
}

void PromptSequenceEditor::setSequence(const PromptSequence &seq)
{
    m_seq = seq;
    refreshList();
}

void PromptSequenceEditor::refreshList()
{
    m_list->clear();
    const auto &steps = m_seq.steps();
    for (int i = 0; i < steps.size(); ++i) {
        const PromptStep &s = steps[i];
        QString title = s.tmpl().section('\n',0,0).trimmed(); // первая строка
        if (title.isEmpty()) title = QString("Шаг %1").arg(i+1);
        QListWidgetItem *it = new QListWidgetItem(title, m_list);
        it->setData(Qt::UserRole, i);
    }
}

/* ---------- Слот‑обработчики ---------- */
void PromptSequenceEditor::onAddStep()
{
    PromptStepDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_seq.addStep(dlg.step());
        refreshList();
        emit sequenceChanged(m_seq);
    }
}
void PromptSequenceEditor::onEditStep()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();

    PromptStepDialog dlg(this);
    dlg.setStep(m_seq.steps()[idx]);
    if (dlg.exec() == QDialog::Accepted) {
        // заменяем шаг
        auto steps = m_seq.steps();
        steps[idx] = dlg.step();
        // Перезаписываем всё (прямой доступ к QVector)
        const_cast<QVector<PromptStep>&>(m_seq.steps()) = steps;
        refreshList();
        emit sequenceChanged(m_seq);
    }
}
void PromptSequenceEditor::onRemoveStep()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();
    m_seq.removeStep(idx);
    refreshList();
    emit sequenceChanged(m_seq);
}
void PromptSequenceEditor::onMoveUp()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();
    if (idx == 0) return;
    m_seq.moveStep(idx, idx-1);
    refreshList();
    m_list->setCurrentRow(idx-1);
    emit sequenceChanged(m_seq);
}
void PromptSequenceEditor::onMoveDown()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();
    if (idx >= m_seq.steps().size()-1) return;
    m_seq.moveStep(idx, idx+1);
    refreshList();
    m_list->setCurrentRow(idx+1);
    emit sequenceChanged(m_seq);
}
