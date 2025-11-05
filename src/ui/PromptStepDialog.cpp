#include "PromptStepDialog.h"
#include "CommandRegistry.h"
#include "PromptResolver.h"   // ← новый заголовок
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <StyledButton.h>
#include <QListWidgetItem>
#include <QMessageBox>
#include "customWidgets/MemoryComboBox.h"


PromptStepDialog::PromptStepDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Редактирование шага");
    resize(600, 460);

    auto *v = new QVBoxLayout(this);

    /* ---------- Кнопка вставки резолвера ---------- */
    auto *hRes = new QHBoxLayout();
    m_resolverCombo = new MemoryComboBox("promptStepResolver", this);
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

    /* ---------- чек‑бокс «Зациклить шаг» ---------- */
    m_loopCheck = new QCheckBox("Зациклить шаг (повторять до остановки)", this);
    v->addWidget(m_loopCheck);

    /* ---------- чек‑бокс «Сделать саммаризацию» ---------- */
    m_summarizeCheck = new QCheckBox("Сделать саммаризацию перед переходом (резюме выполненной работы)", this);
    m_summarizeCheck->setChecked(true); // Default to checked
    v->addWidget(m_summarizeCheck);

    /* ---------- Кнопка добавления ссылки на последовательность ---------- */
    m_addSequenceLinkBtn = new StyledButton("Добавить ссылку на последовательность", this);
    v->addWidget(m_addSequenceLinkBtn);
    connect(m_addSequenceLinkBtn, &StyledButton::clicked, this, &PromptStepDialog::onAddSequenceLink);

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

    /* флаг зацикливания */
    m_loopCheck->setChecked(step.loop());
    
    /* По умолчанию включаем саммаризацию (если не указано иное в шаблоне) */
    m_summarizeCheck->setChecked(true);
    // TODO: Parse template to check if summarize flag is set in CALL_SEQUENCE command
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

void PromptStepDialog::setSequenceManager(PromptSequenceManager* seqMgr)
{
    m_seqMgr = seqMgr;
}

void PromptStepDialog::setCurrentSequencePath(const QString& sequencePath)
{
    if (!sequencePath.isEmpty()) {
        QFileInfo fileInfo(sequencePath);
        m_templateDir = fileInfo.absolutePath(); // Use the directory of the current sequence
    } else {
        m_templateDir.clear();
    }
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

    /* сохраняем флаг зацикливания */
    m_step.setLoop(m_loopCheck->isChecked());

    accept();
}

void PromptStepDialog::onAddSequenceLink()
{
    if (!m_seqMgr) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Менеджер последовательностей не установлен"));
        return;
    }

    // Create a dialog to select sequences
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Выберите последовательность для ссылки"));
    dlg.resize(500, 400);

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);

    // Create a list with checkboxes for each sequence
    QListWidget *seqList = new QListWidget(&dlg);
    
    // If we have a template directory (current sequence directory), get all .seq.json files from there
    if (!m_templateDir.isEmpty() && QDir(m_templateDir).exists()) {
        QDir dir(m_templateDir);
        QStringList filters = {"*.seq.json"};
        QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
        
        for (const QFileInfo& fileInfo : fileInfoList) {
            QString seqPath = fileInfo.absoluteFilePath();
            
            // Load the sequence from file to get its name and description
            QFile seqFile(seqPath);
            if (!seqFile.open(QIODevice::ReadOnly)) {
                continue;
            }
            
            QByteArray data = seqFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject()) {
                continue;  // Skip invalid JSON files
            }
            
            PromptSequence seq = PromptSequence::fromJson(doc.object());
            QString display = seq.name();
            if (!seq.description().isEmpty())
                display += " – " + seq.description();
                
            QListWidgetItem *item = new QListWidgetItem(display, seqList);
            item->setData(Qt::UserRole, fileInfo.baseName());  // Store the filename without extension as requested
            item->setCheckState(Qt::Unchecked);                // Initially unchecked
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        }
    } else {
        // Fallback: show all sequences if we don't have a template directory
        const QMap<QString, PromptSequence> &allSequences = m_seqMgr->sequences();
        
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            const PromptSequence &seq = it.value();
            QString display = seq.name();
            if (!seq.description().isEmpty())
                display += " – " + seq.description();
                
            QListWidgetItem *item = new QListWidgetItem(display, seqList);
            QFileInfo fileInfo(it.key());  // it.key() is the file path
            item->setData(Qt::UserRole, fileInfo.baseName());  // Store the filename without extension
            item->setCheckState(Qt::Unchecked);                // Initially unchecked
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        }
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
            QString seqBaseName = item->data(Qt::UserRole).toString();  // This is already without extension
            QString seqDescription = seqBaseName;                       // Use the same name as description
            
            // Add the OPEN_SEQUENCE reference to the template with both name (without extension) and description
            QString currentTemplate = m_templateEdit->toPlainText();
            if (!currentTemplate.contains(QString("{{OPEN_SEQUENCE:%1").arg(seqBaseName))) {
                QString newTemplate = currentTemplate + QString("\n{{OPEN_SEQUENCE:%1,%2}}\n").arg(seqBaseName, seqDescription);
                m_templateEdit->setPlainText(newTemplate);
            }
        }
    }
}
