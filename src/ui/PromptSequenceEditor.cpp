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

    // Add sequence reference section
    mainLay->addWidget(new QLabel(tr("Ссылки на другие последовательности:")));
    
    QHBoxLayout *refBtnLay = new QHBoxLayout();
    m_addRefBtn = new StyledButton(tr("Добавить ссылку на последовательность"), this);
    refBtnLay->addWidget(m_addRefBtn);
    refBtnLay->addStretch();
    mainLay->addLayout(refBtnLay);
    
    m_refList = new QListWidget(this);
    mainLay->addWidget(m_refList, 0);

    /* ────────────────────── Сигналы ──────────────────────── */
    connect(m_addBtn,   &StyledButton::clicked, this, &PromptSequenceEditor::onAddStep);
    connect(m_editBtn,  &StyledButton::clicked, this, &PromptSequenceEditor::onEditStep);
    connect(m_delBtn,   &StyledButton::clicked, this, &PromptSequenceEditor::onRemoveStep);
    connect(m_upBtn,    &StyledButton::clicked, this, &PromptSequenceEditor::onMoveUp);
    connect(m_downBtn,  &StyledButton::clicked, this, &PromptSequenceEditor::onMoveDown);
    connect(m_list,     &QListWidget::itemDoubleClicked, this, &PromptSequenceEditor::onEditStep);

    connect(m_nameEdit,  &StyleTextEdit::textChanged,   this, &PromptSequenceEditor::onNameChanged);
    connect(m_descEdit,  &StyleTextEdit::textChanged,   this, &PromptSequenceEditor::onDescriptionChanged);
    
    connect(m_addRefBtn, &StyledButton::clicked, this, &PromptSequenceEditor::onAddSequenceReference);
}

/* ────────────────────────────────────── Методы ────────────────────────────────────── */

void PromptSequenceEditor::setSequence(const PromptSequence &seq)
{
    m_seq = seq;
    m_nameEdit->setText(seq.name());
    m_descEdit->setPlainText(seq.description());
    refreshList();
    updateSequenceReferenceList();  // Update the reference list when sequence is set
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
    updateSequenceReferenceList();
}

void PromptSequenceEditor::onDescriptionChanged()
{
    emit sequenceChanged(sequence());
    updateSequenceReferenceList();
}

void PromptSequenceEditor::setSequenceManager(PromptSequenceManager *seqMgr)
{
    m_seqMgr = seqMgr;
}

void PromptSequenceEditor::onAddSequenceReference()
{
    if (!m_seqMgr) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Менеджер последовательностей не установлен"));
        return;
    }

    // Create a dialog to select sequences
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Выберите последовательности для ссылки"));
    dlg.resize(500, 400);

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);

    // Create a list with checkboxes for each sequence
    QListWidget *seqList = new QListWidget(&dlg);
    const QMap<QString, PromptSequence> &allSequences = m_seqMgr->sequences();
    
    for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
        const PromptSequence &seq = it.value();
        QString display = seq.name();
        if (!seq.description().isEmpty())
            display += " – " + seq.description();
            
        QListWidgetItem *item = new QListWidgetItem(display, seqList);
        item->setData(Qt::UserRole, seq.name());  // Store the sequence name
        item->setCheckState(Qt::Unchecked);       // Initially unchecked
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    }

    dlgLayout->addWidget(seqList);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    dlgLayout->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // Process selected sequences
    for (int i = 0; i < seqList->count(); ++i) {
        QListWidgetItem *item = seqList->item(i);
        if (item->checkState() == Qt::Checked) {
            QString seqName = item->data(Qt::UserRole).toString();
            
            // Add the sequence reference to our reference list
            QListWidgetItem *refItem = new QListWidgetItem(QString("→ %1").arg(seqName), m_refList);
            refItem->setData(Qt::UserRole, seqName);
            refItem->setFlags(refItem->flags() | Qt::ItemIsUserCheckable);
            refItem->setCheckState(Qt::Checked); // Checked by default
            
            // Find an existing step that has an OPEN_SEQUENCE reference or create a new one
            bool foundStepWithRef = false;
            auto allCurrentSteps = m_seq.steps();
            QVector<PromptStep> updatedSteps = allCurrentSteps;  // Create a modifiable copy
            
            for (int i = 0; i < updatedSteps.size(); ++i) {
                if (updatedSteps[i].tmpl().contains("{{OPEN_SEQUENCE:")) {
                    // Append to existing reference step
                    QString currentTemplate = updatedSteps[i].tmpl();
                    if (!currentTemplate.contains(QString("{{OPEN_SEQUENCE:%1").arg(seqName))) {
                        updatedSteps[i].setTemplate(currentTemplate + QString("{{OPEN_SEQUENCE:%1}}\n").arg(seqName));
                        foundStepWithRef = true;
                        break;
                    }
                }
            }
            
            // If no step with references was found, create a new step for references
            if (!foundStepWithRef) {
                PromptStep refStep;
                refStep.setTemplate(QString("{{OPEN_SEQUENCE:%1}}\n").arg(seqName));
                refStep.setAllowedCommands(QStringList{});  // No specific commands needed for this step
                updatedSteps.append(refStep);
            }
            
            // Recreate the sequence with updated steps
            PromptSequence newSeq;
            newSeq.setName(m_seq.name());
            newSeq.setDescription(m_seq.description());
            
            for (const auto &step : updatedSteps) {
                newSeq.addStep(step);
            }
            m_seq = newSeq; // Update the member variable
        }
    }
    
    refreshList(); // Refresh the main step list
    emit sequenceChanged(sequence());
}

void PromptSequenceEditor::updateSequenceReferenceList()
{
    m_refList->clear();
    const QMap<QString, PromptSequence> &allSequences = m_seqMgr->sequences();
    
    // Check each step for OPEN_SEQUENCE references
    for (const PromptStep &step : m_seq.steps()) {
        QString templateContent = step.tmpl();
        
        // Find all OPEN_SEQUENCE references in the template
        QRegularExpression regex(R"(\{\{OPEN_SEQUENCE:([^,}]+),?([^}]*)\}\})");
        QRegularExpressionMatchIterator iter = regex.globalMatch(templateContent);
        
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            QString targetSequence = match.captured(1).trimmed();
            
            // Check if the target sequence exists
            bool targetExists = false;
            for (auto checkIt = allSequences.constBegin(); checkIt != allSequences.constEnd(); ++checkIt) {
                if (checkIt.value().name() == targetSequence) {
                    targetExists = true;
                    break;
                }
            }
            
            if (targetExists) {
                QListWidgetItem *item = new QListWidgetItem(QString("→ %1").arg(targetSequence), m_refList);
                item->setData(Qt::UserRole, targetSequence);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Checked);
            }
        }
    }
}
