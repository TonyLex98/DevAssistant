#include "PromptStepDialog.h"
#include "CommandRegistry.h"
#include "PromptResolver.h"   // ← новый заголовок
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QListWidgetItem>
#include <StyledButton.h>
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

    /* ---------- Кнопка добавления резолвера OPEN_SEQUENCE_INFO ---------- */
    m_addSequenceInfoBtn = new StyledButton("Вставить {{OPEN_SEQUENCE_INFO}}", this);
    v->addWidget(m_addSequenceInfoBtn);
    connect(m_addSequenceInfoBtn, &StyledButton::clicked, this, &PromptStepDialog::onAddSequenceInfo);
    
    /* ---------- Добавляем список доступных последовательностей с чекбоксами ---------- */
    QLabel *seqLabel = new QLabel("Доступные последовательности в шаблоне:", this);
    v->addWidget(seqLabel);
    
    m_sequenceListWidget = new QListWidget(this);
    m_sequenceListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    v->addWidget(m_sequenceListWidget, 0); // Small stretch factor so it doesn't take too much space
    
    /* ---------- Загружаем доступные последовательности ---------- */
    loadAvailableSequences();
    
    /* ---------- Подключаем сигнал для отслеживания изменений в списке ---------- */
    connect(m_sequenceListWidget, &QListWidget::itemChanged, this, &PromptStepDialog::onSequenceItemChanged);

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
    // Reload available sequences after sequence manager is set
    loadAvailableSequences();
}

void PromptStepDialog::setCurrentSequencePath(const QString& sequencePath)
{
    if (!sequencePath.isEmpty()) {
        QFileInfo fileInfo(sequencePath);
        m_templateDir = fileInfo.absolutePath(); // Use the directory of the current sequence
        
        // Use the base filename (without extension) as the sequence name for targeting purposes
        // This ensures consistency with the sequenceTargets structure which uses file base names
        m_currentSequenceName = fileInfo.baseName();
    } else {
        m_templateDir.clear();
        m_currentSequenceName.clear();
    }
    // Reload available sequences after template directory and sequence name are set/changed
    loadAvailableSequences();
}

void PromptStepDialog::setCurrentSequenceName(const QString& sequenceName)
{
    m_currentSequenceName = sequenceName;
    // Reload available sequences after sequence name is set/changed
    loadAvailableSequences();
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



void PromptStepDialog::onAddSequenceInfo()
{
    if (!m_templateEdit) return;
    
    QTextCursor cur = m_templateEdit->textCursor();
    cur.insertText("{{OPEN_SEQUENCE_INFO}}");
    m_templateEdit->setTextCursor(cur);
}

void PromptStepDialog::onSequenceItemChanged(QListWidgetItem *item)
{
    Q_UNUSED(item);
    qDebug() << "[onSequenceItemChanged] Slot called for item:" << (item ? item->text() : "NULL");
    qDebug() << "[onSequenceItemChanged] Item check state:" << item->checkState();
    
    // Update the main template file based on the checked items
    if (!m_templateDir.isEmpty() && QDir(m_templateDir).exists()) {
        QString mainTemplatePath = QDir(m_templateDir).filePath("template.templ.json");
        
        if (QFile::exists(mainTemplatePath)) {
            // Load the existing template
            QFile templateFile(mainTemplatePath);
            if (!templateFile.open(QIODevice::ReadOnly)) {
                qDebug() << "[onSequenceItemChanged] Could not read template file:" << mainTemplatePath;
                return;
            }
            
            QByteArray templateData = templateFile.readAll();
            QJsonDocument templateDoc = QJsonDocument::fromJson(templateData);
            templateFile.close();
            
            if (!templateDoc.isObject()) {
                qDebug() << "[onSequenceItemChanged] Invalid JSON in template file";
                return;
            }
            
            QJsonObject templateObj = templateDoc.object();
            
            // Create a map of sequence targets from the UI
            QJsonObject sequenceTargets = templateObj.value("sequenceTargets").toObject();
            
            // For the current sequence, update its targets
            QJsonArray currentTargets;
            
            for (int i = 0; i < m_sequenceListWidget->count(); ++i) {
                QListWidgetItem *listItem = m_sequenceListWidget->item(i);
                if (listItem->checkState() == Qt::Checked) {
                    QString seqFileName = listItem->data(Qt::UserRole).toString();
                    qDebug() << "[onSequenceItemChanged] Found checked item:" << seqFileName;
                    
                    // Add to current sequence's targets
                    currentTargets.append(seqFileName);
                }
            }
            
            // Store the targets for the current sequence
            if (!m_currentSequenceName.isEmpty()) {
                if (!currentTargets.isEmpty()) {
                    // Save targets for this sequence
                    sequenceTargets[m_currentSequenceName] = currentTargets;
                } else {
                    // If no targets, remove the entry if it exists
                    if (sequenceTargets.contains(m_currentSequenceName)) {
                        sequenceTargets.remove(m_currentSequenceName);
                    }
                }
            }
            
            templateObj["sequenceTargets"] = sequenceTargets;
            
            // Write the updated template back to the file
            QJsonDocument updatedDoc(templateObj);
            QFile outFile(mainTemplatePath);
            if (outFile.open(QIODevice::WriteOnly)) {
                outFile.write(updatedDoc.toJson(QJsonDocument::Indented));
                outFile.close();
                qDebug() << "[onSequenceItemChanged] Updated template file with sequence targets for sequence:" << m_currentSequenceName;
            } else {
                qDebug() << "[onSequenceItemChanged] Could not write to template file:" << mainTemplatePath;
            }
        } else {
            qDebug() << "[onSequenceItemChanged] Main template file does not exist, creating default structure...";
            
            // Create a basic template file structure if it doesn't exist
            QJsonObject templateObj;
            templateObj["name"] = QFileInfo(m_templateDir).fileName();  // Use directory name as template name
            templateObj["description"] = "Template for sequences in this directory";
            
            // Create sequence targets if we have a current sequence and checked items
            QJsonObject sequenceTargets;
            
            if (!m_currentSequenceName.isEmpty()) {
                QJsonArray currentTargets;
                
                for (int i = 0; i < m_sequenceListWidget->count(); ++i) {
                    QListWidgetItem *listItem = m_sequenceListWidget->item(i);
                    if (listItem->checkState() == Qt::Checked) {
                        QString seqFileName = listItem->data(Qt::UserRole).toString();
                        currentTargets.append(seqFileName);
                    }
                }
                
                if (!currentTargets.isEmpty()) {
                    sequenceTargets[m_currentSequenceName] = currentTargets;
                }
            }
            
            if (!sequenceTargets.isEmpty()) {
                templateObj["sequenceTargets"] = sequenceTargets;
            }
            
            // Write the new template structure
            QJsonDocument newDoc(templateObj);
            QFile outFile(mainTemplatePath);
            if (outFile.open(QIODevice::WriteOnly)) {
                outFile.write(newDoc.toJson(QJsonDocument::Indented));
                outFile.close();
                qDebug() << "[onSequenceItemChanged] Created new template file with sequence targets for sequence:" << m_currentSequenceName;
            } else {
                qDebug() << "[onSequenceItemChanged] Could not create template file:" << mainTemplatePath;
            }
        }
    } else {
        qDebug() << "[onSequenceItemChanged] Template directory is empty or doesn't exist";
    }
}

void PromptStepDialog::loadAvailableSequences()
{
    qDebug() << "[loadAvailableSequences] Starting to load available sequences";
    qDebug() << "[loadAvailableSequences] m_seqMgr is null:" << (m_seqMgr == nullptr);
    qDebug() << "[loadAvailableSequences] m_sequenceListWidget is null:" << (m_sequenceListWidget == nullptr);
    qDebug() << "[loadAvailableSequences] m_templateDir:" << m_templateDir;
    
    if (!m_sequenceListWidget) {
        qDebug() << "[loadAvailableSequences] m_sequenceListWidget is null, returning";
        return;
    }
    
    m_sequenceListWidget->clear();
    
    // Get the current template content to check for existing references
    QString currentTemplate = m_templateEdit ? m_templateEdit->toPlainText() : QString();
    
    // Look for the main template file to read sequence links
    qDebug() << "[loadAvailableSequences] Checking for main template file. m_templateDir:" << m_templateDir;
    bool hasMainTemplate = false;
    if (!m_templateDir.isEmpty() && QDir(m_templateDir).exists()) {
        QString mainTemplatePath = QDir(m_templateDir).filePath("template.templ.json");
        
        if (QFile::exists(mainTemplatePath)) {
            // Load the main template to get sequence links
            QFile templateFile(mainTemplatePath);
            if (templateFile.open(QIODevice::ReadOnly)) {
                QByteArray templateData = templateFile.readAll();
                QJsonDocument templateDoc = QJsonDocument::fromJson(templateData);
                
                if (templateDoc.isObject()) {
                    QJsonObject templateObj = templateDoc.object();
                    
                    // Look for sequence targets (directed links) in the template
                    if (templateObj.contains("sequenceTargets")) {
                        hasMainTemplate = true;
                        
                        qDebug() << "[loadAvailableSequences] Found sequenceTargets in main template";
                        
                        // Get targets for the current sequence (directed links)
                        QJsonArray currentSequenceTargets;
                        if (!m_currentSequenceName.isEmpty() && templateObj.contains("sequenceTargets")) {
                            QJsonObject sequenceTargets = templateObj.value("sequenceTargets").toObject();
                            if (sequenceTargets.contains(m_currentSequenceName)) {
                                currentSequenceTargets = sequenceTargets.value(m_currentSequenceName).toArray();
                                qDebug() << "[loadAvailableSequences] Current sequence" << m_currentSequenceName << "has" << currentSequenceTargets.size() << "target sequences";
                                
                                for (int i = 0; i < currentSequenceTargets.size(); ++i) {
                                    qDebug() << "[loadAvailableSequences] Target" << i << ":" << currentSequenceTargets[i].toString();
                                }
                            } else {
                                qDebug() << "[loadAvailableSequences] Current sequence" << m_currentSequenceName << "has no target sequences defined";
                            }
                        } else {
                            qDebug() << "[loadAvailableSequences] No current sequence name or sequenceTargets in template";
                        }
                        
                        // Get all sequence files in the directory
                        QDir dir(m_templateDir);
                        QStringList filters = {"*.seq.json"};
                        QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
                        
                        // Process each sequence file and check if it should be checked based on targets
                        QStringList targetSequenceFiles;  // List of sequence files that are targets of current sequence
                        for (const QJsonValue& targetValue : currentSequenceTargets) {
                            targetSequenceFiles.append(targetValue.toString());
                        }
                        
                        // Create a map of file info for faster lookup
                        QMap<QString, QFileInfo> fileInfoMap;
                        for (const QFileInfo& fileInfo : fileInfoList) {
                            QString baseName = fileInfo.baseName();
                            fileInfoMap[baseName] = fileInfo;
                        }
                        
                        // First add the target sequences (checked)
                        for (const QString& targetFile : targetSequenceFiles) {
                            if (fileInfoMap.contains(targetFile)) {
                                QFileInfo fileInfo = fileInfoMap[targetFile];
                                
                                // Load the sequence from file to get its name and description
                                QFile seqFile(fileInfo.absoluteFilePath());
                                if (!seqFile.open(QIODevice::ReadOnly)) {
                                    continue;
                                }
                                
                                QByteArray data = seqFile.readAll();
                                QJsonDocument doc = QJsonDocument::fromJson(data);
                                if (!doc.isObject()) {
                                    continue;  // Skip invalid JSON files
                                }
                                
                                PromptSequence seq = PromptSequence::fromJson(doc.object());
                                qDebug() << "[loadAvailableSequences] Loaded target sequence:" << seq.name() << "file:" << targetFile;
                                
                                QString display = seq.name();
                                if (!seq.description().isEmpty())
                                    display += " – " + seq.description();
                                    
                                QListWidgetItem *item = new QListWidgetItem(display, m_sequenceListWidget);
                                item->setData(Qt::UserRole, targetFile);  // Store the filename without extension
                                item->setCheckState(Qt::Checked);  // Target sequences are checked
                                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                                
                                qDebug() << "[loadAvailableSequences] Added checked target sequence:" << targetFile;
                            }
                        }
                        
                        // Then add the non-target sequences (unchecked)
                        for (const QFileInfo& fileInfo : fileInfoList) {
                            QString seqBaseName = fileInfo.baseName();  // Get filename without extension
                            
                            // Skip if this sequence file is already added as a target
                            if (targetSequenceFiles.contains(seqBaseName)) {
                                continue;  // Already added above as checked
                            }
                            
                            // Load the sequence from file to get its name and description
                            QFile seqFile(fileInfo.absoluteFilePath());
                            if (!seqFile.open(QIODevice::ReadOnly)) {
                                continue;
                            }
                            
                            QByteArray data = seqFile.readAll();
                            QJsonDocument doc = QJsonDocument::fromJson(data);
                            if (!doc.isObject()) {
                                continue;  // Skip invalid JSON files
                            }
                            
                            PromptSequence seq = PromptSequence::fromJson(doc.object());
                            qDebug() << "[loadAvailableSequences] Loaded non-target sequence:" << seq.name();
                            
                            QString display = seq.name();
                            if (!seq.description().isEmpty())
                                display += " – " + seq.description();
                                
                            QListWidgetItem *item = new QListWidgetItem(display, m_sequenceListWidget);
                            item->setData(Qt::UserRole, seqBaseName);  // Store the filename without extension
                            item->setCheckState(Qt::Unchecked);        // Non-target sequences are unchecked
                            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                            
                            qDebug() << "[loadAvailableSequences] Added unchecked non-target item for:" << seqBaseName;
                        }
                    }
                }
            }
        }
    }
    
    // If no main template with links was found, fallback to all sequences in the directory
    if (!hasMainTemplate) {
        // Load sequences directly from the template directory only
        if (!m_templateDir.isEmpty() && QDir(m_templateDir).exists()) {
            QDir dir(m_templateDir);
            QStringList filters = {"*.seq.json"};
            QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
            
            qDebug() << "[loadAvailableSequences] Found" << fileInfoList.size() << "sequence files in template directory:" << m_templateDir;
            
            for (const QFileInfo& fileInfo : fileInfoList) {
                QString seqPath = fileInfo.absoluteFilePath();
                qDebug() << "[loadAvailableSequences] Processing sequence file:" << seqPath;
                
                // Load the sequence from file to get its name and description
                QFile seqFile(seqPath);
                if (!seqFile.open(QIODevice::ReadOnly)) {
                    qDebug() << "[loadAvailableSequences] Could not open file:" << seqPath;
                    continue;
                }
                
                QByteArray data = seqFile.readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data);
                if (!doc.isObject()) {
                    qDebug() << "[loadAvailableSequences] Invalid JSON in file:" << seqPath;
                    continue;  // Skip invalid JSON files
                }
                
                PromptSequence seq = PromptSequence::fromJson(doc.object());
                qDebug() << "[loadAvailableSequences] Loaded sequence:" << seq.name();
                
                QString display = seq.name();
                if (!seq.description().isEmpty())
                    display += " – " + seq.description();
                    
                QListWidgetItem *item = new QListWidgetItem(display, m_sequenceListWidget);
                item->setData(Qt::UserRole, fileInfo.baseName());  // Store the filename without extension
                
                // For the new OPEN_SEQUENCE_INFO system, we could consider all sequences 
                // as "available" in the template, but let's check if the template contains the resolver
                if (currentTemplate.contains("{{OPEN_SEQUENCE_INFO}}")) {
                    // If the template already has OPEN_SEQUENCE_INFO, consider all sequences as "included"
                    item->setCheckState(Qt::Checked);
                } else {
                    // Otherwise start with unchecked
                    item->setCheckState(Qt::Unchecked);
                }
                
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                
                qDebug() << "[loadAvailableSequences] Added item for:" << fileInfo.baseName() << "with check state:" << item->checkState();
            }
        } else {
            qDebug() << "[loadAvailableSequences] Template directory is empty or doesn't exist, trying to find template directory...";
            
            // Try to determine template directory from the sequence manager's root or common template locations
            if (m_seqMgr) {
                QString seqMgrRoot = m_seqMgr->root();
                qDebug() << "[loadAvailableSequences] Sequence manager root:" << seqMgrRoot;
                
                // Check if this is in a template directory structure
                QDir checkDir(seqMgrRoot);
                if (checkDir.exists()) {
                    // Look for template directories within the sequence manager root
                    QFileInfoList dirList = checkDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                    
                    for (const QFileInfo& dirInfo : dirList) {
                        if (dirInfo.fileName() != "PromptSequences") {  // Skip the main sequences folder
                            QDir templateDir(dirInfo.absoluteFilePath());
                            QStringList filters = {"*.seq.json"};
                            QFileInfoList fileInfoList = templateDir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
                            
                            if (!fileInfoList.isEmpty()) {
                                qDebug() << "[loadAvailableSequences] Found template directory with sequences:" << dirInfo.absoluteFilePath();
                                
                                for (const QFileInfo& fileInfo : fileInfoList) {
                                    QString seqPath = fileInfo.absoluteFilePath();
                                    qDebug() << "[loadAvailableSequences] Processing sequence file:" << seqPath;
                                    
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
                                        
                                    QListWidgetItem *item = new QListWidgetItem(display, m_sequenceListWidget);
                                    item->setData(Qt::UserRole, fileInfo.baseName());  // Store the filename without extension
                                    
                                    // Check if the template already has OPEN_SEQUENCE_INFO
                                    if (currentTemplate.contains("{{OPEN_SEQUENCE_INFO}}")) {
                                        item->setCheckState(Qt::Checked);
                                    } else {
                                        item->setCheckState(Qt::Unchecked);
                                    }
                                    
                                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    qDebug() << "[loadAvailableSequences] Final count of items:" << m_sequenceListWidget->count();
}
