#include "PromptSequenceEditor.h"
#include "PromptStepDialog.h"
#include "../core/PromptSequenceManager.h"
#include "CommandRegistry.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <StyledButton.h>
#include <StyleTextEdit.h>
#include <QMessageBox>
#include <QLabel>
#include <QFormLayout>
#include <QLineEdit>
#include <QSplitter>
#include <QSignalBlocker>
#include "customWidgets/StyledListWidget.h"

static QStringList allCommandNames()
{
    QStringList out;
    const auto all = CommandRegistry::all();
    out.reserve(all.size());
    for (Command *c : all) {
        out << c->name();
        delete c;
    }
    return out;
}

PromptSequenceEditor::PromptSequenceEditor(QWidget *parent)
    : QWidget(parent)
{
    /* ────────────────────────────────────── UI ──────────────────────────────────────── */

    auto *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(10);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Sequence name"));
    m_nameEdit->setStyleSheet(
        "QLineEdit {"
        "  border: 1px solid #0f766e;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  font: 10pt \"Segoe UI\";"
        "}"
    );

    m_descEdit = new StyleTextEdit(this);
    m_descEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_descEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_descEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_descEdit->setMinimumHeight(80);
    m_descEdit->setMaximumHeight(140);

    form->addRow(tr("Name:"), m_nameEdit);
    form->addRow(tr("Description:"), m_descEdit);
    mainLay->addLayout(form);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(
        "QSplitter::handle:horizontal {"
        "  width: 10px;"
        "  background: #c8def8;"
        "  border-left: 1px solid #5A9;"
        "  border-right: 1px solid #5A9;"
        "}"
    );
    mainLay->addWidget(splitter, 1);

    auto *left = new QWidget(splitter);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(8);

    leftLay->addWidget(new QLabel(tr("Steps:"), left));

    m_list = new StyledListWidget(left);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLay->addWidget(m_list, 1);

    auto *btnLay = new QHBoxLayout();
    m_addBtn  = new StyledButton(tr("Add"), left);
    m_editBtn = new StyledButton(tr("Edit"), left);
    m_delBtn  = new StyledButton(tr("Delete"), left);
    m_upBtn   = new StyledButton(tr("Up"), left);
    m_downBtn = new StyledButton(tr("Down"), left);

    btnLay->addWidget(m_addBtn);
    btnLay->addWidget(m_editBtn);
    btnLay->addWidget(m_delBtn);
    btnLay->addStretch();
    btnLay->addWidget(m_upBtn);
    btnLay->addWidget(m_downBtn);
    leftLay->addLayout(btnLay);

    splitter->addWidget(left);

    m_stepEditor = new PromptStepDialog(splitter);
    splitter->addWidget(m_stepEditor);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    splitter->setSizes({360, 900});

    /* ────────────────────── Signals ──────────────────────── */
    connect(m_addBtn,   &StyledButton::clicked, this, &PromptSequenceEditor::onAddStep);
    connect(m_editBtn,  &StyledButton::clicked, this, &PromptSequenceEditor::onEditStep);
    connect(m_delBtn,   &StyledButton::clicked, this, &PromptSequenceEditor::onRemoveStep);
    connect(m_upBtn,    &StyledButton::clicked, this, &PromptSequenceEditor::onMoveUp);
    connect(m_downBtn,  &StyledButton::clicked, this, &PromptSequenceEditor::onMoveDown);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) { loadStepToEditor(row); });

    connect(m_nameEdit, &QLineEdit::textChanged, this, &PromptSequenceEditor::onNameChanged);
    connect(m_descEdit, &QTextEdit::textChanged, this, &PromptSequenceEditor::onDescriptionChanged);

    connect(m_stepEditor, &PromptStepDialog::stepEdited, this, [this](const PromptStep& s) {
        if (m_currentStepIdx < 0)
            return;
        m_seq.setStep(m_currentStepIdx, s);
        updateCurrentStepTitle();
        emit sequenceChanged(sequence());
    });

    loadStepToEditor(-1);
}

/* ────────────────────────────────────── Methods ────────────────────────────────────── */

void PromptSequenceEditor::setSequence(const PromptSequence &seq)
{
    m_seq = seq;
    m_nameEdit->setText(seq.name());
    m_descEdit->setPlainText(seq.description());
    refreshList();
    if (m_seq.steps().isEmpty())
        loadStepToEditor(-1);
    else
        m_list->setCurrentRow(0);
}

PromptSequence PromptSequenceEditor::sequence() const
{
    PromptSequence seq = m_seq;
    seq.setName(m_nameEdit->text());
    seq.setDescription(m_descEdit->toPlainText());
    return seq;
}

void PromptSequenceEditor::refreshList()
{
    const int preserveRow = m_list ? m_list->currentRow() : -1;
    m_list->clear();
    const auto &steps = m_seq.steps();
    for (int i = 0; i < steps.size(); ++i) {
        const PromptStep &s = steps[i];
        QString title = s.tmpl().section('\n', 0, 0).trimmed(); // first line of the template
        if (title.isEmpty()) title = tr("Step %1").arg(i + 1);
        QListWidgetItem *it = new QListWidgetItem(title, m_list);
        it->setData(Qt::UserRole, i);
    }
    if (preserveRow >= 0 && preserveRow < m_list->count())
        m_list->setCurrentRow(preserveRow);
}

/* ────────────────────── Slots ──────────────────────── */

void PromptSequenceEditor::onAddStep()
{
    const int curRow = m_list ? m_list->currentRow() : -1;
    const int insertAt = (curRow >= 0) ? (curRow + 1) : m_seq.steps().size();

    PromptStep step;
    step.setAllowedCommands(allCommandNames());
    m_seq.insertStep(insertAt, step);

    refreshList();
    m_list->setCurrentRow(insertAt);
    emit sequenceChanged(sequence());
}

void PromptSequenceEditor::onEditStep()
{
    if (m_stepEditor)
        m_stepEditor->focusTemplate();
}

void PromptSequenceEditor::onRemoveStep()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    const int idx = m_list->currentRow();
    m_seq.removeStep(idx);
    refreshList();
    emit sequenceChanged(sequence());

    if (m_seq.steps().isEmpty())
        loadStepToEditor(-1);
    else
        m_list->setCurrentRow(qMin(idx, m_list->count() - 1));
}

void PromptSequenceEditor::onMoveUp()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) return;
    const int idx = m_list->currentRow();
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
    const int idx = m_list->currentRow();
    if (idx >= m_seq.steps().size() - 1) return;
    m_seq.moveStep(idx, idx + 1);
    refreshList();
    m_list->setCurrentRow(idx + 1);
    emit sequenceChanged(sequence());
}

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
    if (m_stepEditor)
        m_stepEditor->setSequenceManager(seqMgr);
}

void PromptSequenceEditor::setCurrentSequencePath(const QString& sequencePath)
{
    m_currentSequencePath = sequencePath;
    if (m_stepEditor)
        m_stepEditor->setCurrentSequencePath(sequencePath);
}

void PromptSequenceEditor::loadStepToEditor(int idx)
{
    m_currentStepIdx = idx;

    const bool hasStep = (idx >= 0 && idx < m_seq.steps().size());
    if (m_stepEditor) {
        m_stepEditor->setEnabled(hasStep);
        if (hasStep) {
            QSignalBlocker blocker(m_stepEditor);
            m_stepEditor->setStep(m_seq.steps().at(idx));
        }
    }

    if (!hasStep) {
        if (m_editBtn) m_editBtn->setEnabled(false);
        if (m_delBtn) m_delBtn->setEnabled(false);
        if (m_upBtn) m_upBtn->setEnabled(false);
        if (m_downBtn) m_downBtn->setEnabled(false);
        return;
    }

    if (m_editBtn) m_editBtn->setEnabled(true);
    if (m_delBtn) m_delBtn->setEnabled(true);
    if (m_upBtn) m_upBtn->setEnabled(idx > 0);
    if (m_downBtn) m_downBtn->setEnabled(idx < m_seq.steps().size() - 1);
}

void PromptSequenceEditor::updateCurrentStepTitle()
{
    if (!m_list || m_currentStepIdx < 0 || m_currentStepIdx >= m_list->count())
        return;
    const PromptStep &s = m_seq.steps().at(m_currentStepIdx);
    QString title = s.tmpl().section('\n', 0, 0).trimmed();
    if (title.isEmpty())
        title = tr("Step %1").arg(m_currentStepIdx + 1);
    if (auto *it = m_list->item(m_currentStepIdx))
        it->setText(title);
}


