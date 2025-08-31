#include "PromptStepDialog.h"
#include "CommandRegistry.h"
#include "PromptResolver.h"   // ← новый заголовок
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <StyledButton.h>
#include <QListWidgetItem>

PromptStepDialog::PromptStepDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Редактирование шага");
    resize(600, 460);

    auto *v = new QVBoxLayout(this);

    /* ---------- Кнопка вставки резолвера ---------- */
    auto *hRes = new QHBoxLayout();
    m_resolverCombo = new QComboBox(this);
    m_insertBtn = new StyledButton("Вставить", this);
    hRes->addWidget(m_resolverCombo);
    hRes->addWidget(m_insertBtn);
    hRes->addStretch();
    v->addLayout(hRes);

    /* Заполняем список доступных резолверов (описания) */
    for (const auto &info : PromptResolver::availableResolvers()) {
        m_resolverCombo->addItem(info.description, info.token);
    }
    connect(m_insertBtn, &StyledButton::clicked,
            this, &PromptStepDialog::onInsertClicked);
    /* ------------------------------------------------ */

    m_templateEdit = new StyleTextEdit(this);
    v->addWidget(m_templateEdit, 2);

    /* Список команд с чек‑боксами */
    m_cmdList = new QListWidget(this);
    m_cmdList->setSelectionMode(QAbstractItemView::NoSelection);
    v->addWidget(m_cmdList, 1);

    /* Заполняем списком всех известных команд */
    for (Command *c : CommandRegistry::all()) {
        QListWidgetItem *it = new QListWidgetItem(c->name(), m_cmdList);
        it->setCheckState(Qt::Unchecked);
        it->setData(Qt::UserRole, c->name());
        delete c; // шаблон нам больше не нужен
    }

    /* Кнопки ОК/Отмена */
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    v->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &PromptStepDialog::onOk);
    connect(bb, &QDialogButtonBox::rejected, this, &PromptStepDialog::reject);
}

void PromptStepDialog::setStep(const PromptStep &step)
{
    m_step = step;
    m_templateEdit->setPlainText(step.tmpl());

    /* Ставим чек‑боксы согласно списку разрешённых команд */
    for (int i = 0; i < m_cmdList->count(); ++i) {
        QListWidgetItem *it = m_cmdList->item(i);
        QString name = it->data(Qt::UserRole).toString();
        it->setCheckState(step.allowedCommands().contains(name) ?
                          Qt::Checked : Qt::Unchecked);
    }
}

void PromptStepDialog::onInsertClicked()
{
    if (!m_templateEdit) return;
    int idx = m_resolverCombo->currentIndex();
    if (idx < 0) return;
    QString token = m_resolverCombo->currentData().toString(); // это {{TOKEN}}
    QTextCursor cur = m_templateEdit->textCursor();
    cur.insertText("{{" + token + "}}");
    m_templateEdit->setTextCursor(cur);
}

void PromptStepDialog::onOk()
{
    m_step.setTemplate(m_templateEdit->toPlainText());

    QStringList allowed;
    for (int i = 0; i < m_cmdList->count(); ++i) {
        QListWidgetItem *it = m_cmdList->item(i);
        if (it->checkState() == Qt::Checked)
            allowed << it->data(Qt::UserRole).toString();
    }
    m_step.setAllowedCommands(allowed);
    accept();
}
