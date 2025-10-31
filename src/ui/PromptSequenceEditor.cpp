#include "PromptSequenceEditor.h"
#include "PromptStepDialog.h"
#include "../core/PromptSequenceManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <StyledButton.h>
#include <QLineEdit>
#include <StyleTextEdit.h>
#include <QMessageBox>
#include <QFormLayout>
#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>

PromptSequenceEditor::PromptSequenceEditor(QWidget *parent)
    : QWidget(parent)
{
    /* ────────────────────────────────────── UI ──────────────────────────────────────── */

    QVBoxLayout *mainLay = new QVBoxLayout(this);

    /* 1. Поля имени/описания */
    m_nameEdit = new StyleTextEdit(this);
    m_descEdit = new StyleTextEdit(this);
    m_descEdit->setMinimumHeight(80);


    mainLay->addWidget(new QLabel(tr("Имя:")));
    mainLay->addWidget(m_nameEdit);

    mainLay->addWidget(new QLabel(tr("Описание:")));
    mainLay->addWidget(m_descEdit);

    /* 2. Список шагов */
    m_list = new QListWidget(this);
    mainLay->addWidget(m_list, 1);

    /* 3. Кнопки управления */
    QHBoxLayout *btnLay = new QHBoxLayout;
    m_addBtn  = new StyledButton(tr("Add"), this);
    m_editBtn = new StyledButton(tr("Edit"), this);
    m_delBtn  = new StyledButton(tr("Delete"), this);
    m_upBtn   = new StyledButton(tr("Up"), this);
    m_downBtn = new StyledButton(tr("Down"), this);

    btnLay->addWidget(m_addBtn);
    btnLay->addWidget(m_editBtn);
    btnLay->addWidget(m_delBtn);
    btnLay->addStretch();
    btnLay->addWidget(m_upBtn);
    btnLay->addWidget(m_downBtn);
    mainLay->addLayout(btnLay);

    /* ────────────────────── Сигналы ──────────────────────── */
    connect(m_addBtn,   &StyledButton::clicked, this, &PromptSequenceEditor::onAddStep);
    connect(m_editBtn,  &StyledButton::clicked, this, &PromptSequenceEditor::onEditStep);
    connect(m_delBtn,   &StyledButton::clicked, this, &PromptSequenceEditor::onRemoveStep);
    connect(m_upBtn,    &StyledButton::clicked, this, &PromptSequenceEditor::onMoveUp);
    connect(m_downBtn,  &StyledButton::clicked, this, &PromptSequenceEditor::onMoveDown);
    connect(m_list,     &QListWidget::itemDoubleClicked, this, &PromptSequenceEditor::onEditStep);

    connect(m_nameEdit,  &StyleTextEdit::textChanged,   this, &PromptSequenceEditor::onNameChanged);
    connect(m_descEdit,  &StyleTextEdit::textChanged,   this, &PromptSequenceEditor::onDescriptionChanged);
}

/* ────────────────────────────────────── Методы ────────────────────────────────────── */

void PromptSequenceEditor::setSequence(const PromptSequence &seq)
{
    m_seq = seq;
    m_nameEdit->setText(seq.name());
    m_descEdit->setPlainText(seq.description());
    refreshList();
}

PromptSequence PromptSequenceEditor::sequence() const
{
    PromptSequence seq = m_seq;
    seq.setName(m_nameEdit->toPlainText());
    seq.setDescription(m_descEdit->toPlainText());
    return seq;
}

void PromptSequenceEditor::refreshList()
{
    m_list->clear();
    const auto &steps = m_seq.steps();
    for (int i = 0; i < steps.size(); ++i) {
        const PromptStep &s = steps[i];
        QString title = s.tmpl().section('\n', 0, 0).trimmed(); // первая строка шаблона
        if (title.isEmpty()) title = tr("Шаг %1").arg(i + 1);
        QListWidgetItem *it = new QListWidgetItem(title, m_list);
        it->setData(Qt::UserRole, i);
    }
}

/* ────────────────────── Слоты ──────────────────────── */

void PromptSequenceEditor::onAddStep()
{
    PromptStepDialog dlg(this);
    dlg.setSequenceManager(m_seqMgr);  // Pass the sequence manager
    if (dlg.exec() == QDialog::Accepted) {
        m_seq.addStep(dlg.step());
        refreshList();
        emit sequenceChanged(sequence());
    }
}

void PromptSequenceEditor::onEditStep()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();

    PromptStepDialog dlg(this);
    dlg.setStep(m_seq.steps()[idx]);
    dlg.setSequenceManager(m_seqMgr);  // Pass the sequence manager

    if (dlg.exec() == QDialog::Accepted) {
        // Заменяем шаг, но без пересоздания объекта PromptSequence
        auto steps = m_seq.steps();
        steps[idx] = dlg.step();
        const_cast<QVector<PromptStep>&>(m_seq.steps()) = steps;

        refreshList();
        emit sequenceChanged(sequence());
    }
}

void PromptSequenceEditor::onRemoveStep()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();
    m_seq.removeStep(idx);
    refreshList();
    emit sequenceChanged(sequence());
}

void PromptSequenceEditor::onMoveUp()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();
    if (idx == 0) return;
    m_seq.moveStep(idx, idx - 1);
    refreshList();
    m_list->setCurrentRow(idx - 1);
    emit sequenceChanged(sequence());
}

void PromptSequenceEditor::onMoveDown()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();
    if (idx >= m_seq.steps().size() - 1) return;
    m_seq.moveStep(idx, idx + 1);
    refreshList();
    m_list->setCurrentRow(idx + 1);
    emit sequenceChanged(sequence());
}

/* Сигналы из полей имени/описания */
void PromptSequenceEditor::onNameChanged()
{
    emit sequenceChanged(sequence());
}

void PromptSequenceEditor::onDescriptionChanged()
{
    emit sequenceChanged(sequence());
}

void PromptSequenceEditor::setSequenceManager(PromptSequenceManager *seqMgr)
{
    m_seqMgr = seqMgr;
}


