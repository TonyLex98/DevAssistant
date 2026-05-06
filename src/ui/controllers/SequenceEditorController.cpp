#include "SequenceEditorController.h"
#include "../MainWindow.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QFile>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QObject>
#include <PromptSequenceEditor.h>

using namespace std::literals;

SequenceEditorController::SequenceEditorController(MainWindow* mw)
    : m_mw(mw)
{
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::openSequenceGraph()
{
    qDebug() << "[MainWindow] openSequenceGraph called";

    // Refresh managers to ensure we have the latest data
    m_mw->m_templateMgr.refresh();
    refreshAllSequences();
    ensureDefaultSequence();

    if (m_mw->m_seqGraphWindow) {
        m_mw->m_seqGraphWindow->setSequenceManager(&m_mw->m_seqMgr);
        m_mw->m_seqGraphWindow->setTemplateManager(&m_mw->m_templateMgr);
        m_mw->m_seqGraphWindow->rebuildGraph();
        m_mw->m_seqGraphWindow->setFocus();
        m_mw->m_seqGraphWindow->setVisible(true);
        updateSequenceGraphTemplateSelection();
    }
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::onChatTemplateSelectionChanged(const QString &templatePath)
{
    QString normalizedPath = templatePath;
    if (normalizedPath == "simple_chat")
        normalizedPath.clear();

    if (m_mw->m_currentTemplatePath == normalizedPath)
        return;

    m_mw->m_currentTemplatePath = normalizedPath;
    updateSequenceGraphTemplateSelection();
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::updateSequenceGraphHighlight()
{
    if (m_mw->m_seqGraphWindow && m_mw->m_sequenceMode) {
        m_mw->m_seqGraphWindow->setCurrentSequence(m_mw->m_currentSeq.name());
    }
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::updateSequenceGraphTemplateSelection()
{
    if (m_mw->m_seqGraphWindow) {
        m_mw->m_seqGraphWindow->setTemplateSelectionByPath(m_mw->m_currentTemplatePath);
    }
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::openSequenceEditorForName(const QString &sequenceName)
{
    // Обновим кэш, чтобы включить все последовательности из папок шаблонов
    refreshAllSequences();

    // Find the sequence by name using the new method
    m_mw->log("DEBUG: Searching for sequence name: " + sequenceName + " in openSequenceEditorForName");
    QPair<QString, PromptSequence> result = m_mw->m_seqMgr.findSequenceByName(sequenceName);
    QString filePath = result.first;
    PromptSequence seq = result.second;

    if (filePath.isEmpty()) {
        // If not found in main manager, try to find in template directories
        m_mw->log("DEBUG: Sequence NOT FOUND in main manager, searching in template directories...");

        // Search in template directories
        const QStringList& templateDirs = m_mw->m_templateMgr.templateDirs();
        for (const QString& templateDir : templateDirs) {
            QDir dir(templateDir);
            if (dir.exists()) {
                // ищем *.seq.json в папке шаблона
                QStringList filters = {"*.seq.json"};
                QFileInfoList files = dir.entryInfoList(filters,
                                                        QDir::Files | QDir::NoSymLinks,
                                                        QDir::Name);

                for (const QFileInfo& fileInfo : files) {
                    QString path = fileInfo.absoluteFilePath();

                    // Load the sequence from this file and check its name
                    QFile seqFile(path);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            PromptSequence tempSeq = PromptSequence::fromJson(doc.object());
                            if (tempSeq.name() == sequenceName) {
                                // Found the sequence in template directory
                                seq = tempSeq;
                                filePath = path;
                                m_mw->log("DEBUG: Found sequence '" + sequenceName + "' in template directory: " + path);
                                seqFile.close();
                                break;
                            }
                        }
                        seqFile.close();
                    }
                }

                if (!filePath.isEmpty()) {
                    // Found in this template directory, exit the outer loop
                    break;
                }
            }
        }
    }

    if (filePath.isEmpty()) {
        m_mw->log("DEBUG: Sequence NOT FOUND in openSequenceEditorForName. Available sequences:");
        const QMap<QString, PromptSequence>& allSequences_debug = m_mw->m_seqMgr.sequences();
        for (auto it = allSequences_debug.constBegin(); it != allSequences_debug.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.fileName();
            m_mw->log("  - Available sequence name: '" + seqName + "' from file: '" + fileName + "'");
        }
        // Also list template directory contents
        const QStringList& templateDirs = m_mw->m_templateMgr.templateDirs();
        m_mw->log("  Template directories and their sequences:");
        for (const QString& templateDir : templateDirs) {
            QDir dir(templateDir);
            if (dir.exists()) {
                QStringList filters = {"*.seq.json"};
                QFileInfoList files = dir.entryInfoList(filters,
                                                        QDir::Files | QDir::NoSymLinks,
                                                        QDir::Name);
                m_mw->log("    - Template directory: " + templateDir);
                for (const QFileInfo& fileInfo : files) {
                    // Load and get the sequence name
                    QFile seqFile(fileInfo.absoluteFilePath());
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            PromptSequence tempSeq = PromptSequence::fromJson(doc.object());
                            m_mw->log("      - Sequence file: '" + fileInfo.fileName() + "' with internal name: '" + tempSeq.name() + "'");
                        }
                        seqFile.close();
                    }
                }
            }
        }
        QMessageBox::warning(m_mw, "Error", "Sequence not found: " + sequenceName);
        return;
    } else {
        m_mw->log("DEBUG: Successfully found sequence '" + sequenceName + "' at path: " + filePath);
    }

    openSequenceEditorWindow(QObject::tr("Редактор последовательности") + ": " + sequenceName, filePath, seq);
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::openSequenceEditorWindow(const QString& title, const QString& filePath, const PromptSequence& seq)
{
    auto *win = new QWidget(m_mw);
    win->setWindowFlag(Qt::Window, true);
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->setWindowTitle(title);
    win->resize(1280, 820);
    win->setMinimumSize(1024, 700);

    auto *lay = new QVBoxLayout(win);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(10);

    auto *editor = new PromptSequenceEditor(win);
    editor->setSequenceManager(&m_mw->m_seqMgr);
    editor->setSequence(seq);
    editor->setCurrentSequencePath(filePath);
    lay->addWidget(editor, 1);

    auto *btnRow = new QHBoxLayout();
    auto *saveBtn = new StyledButton(QObject::tr("Сохранить"), win);
    auto *closeBtn = new StyledButton(QObject::tr("Закрыть"), win);
    saveBtn->setEnabled(false);
    btnRow->addStretch(1);
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);

    auto refreshAfterSave = [this]() {
        refreshAllSequences();
        ensureDefaultSequence();
        if (m_mw->m_seqGraphWindow) {
            if (m_mw->m_currentTemplatePath.isEmpty()) {
                m_mw->m_seqGraphWindow->rebuildGraph();
            } else {
                m_mw->m_seqGraphWindow->rebuildGraphForTemplate(m_mw->m_currentTemplatePath);
                m_mw->m_seqGraphWindow->setTemplateSelectionByPath(m_mw->m_currentTemplatePath);
            }
        }
    };

    QObject::connect(editor, &PromptSequenceEditor::sequenceChanged, win, [win, saveBtn](const PromptSequence& edited) {
        saveBtn->setEnabled(true);
        win->setWindowTitle(QObject::tr("Редактор последовательности") + ": " + edited.name());
    });

    QObject::connect(saveBtn, &QPushButton::clicked, win, [this, editor, filePath, saveBtn, refreshAfterSave, win]() {
        const PromptSequence edited = editor->sequence();
        if (m_mw->m_seqMgr.save(edited, filePath)) {
            QMessageBox::information(win, QObject::tr("Сохранено"), QObject::tr("Последовательность сохранена."));
            saveBtn->setEnabled(false);
            refreshAfterSave();
        } else {
            QMessageBox::warning(win, QObject::tr("Ошибка"), QObject::tr("Не удалось сохранить."));
        }
    });

    QObject::connect(closeBtn, &QPushButton::clicked, win, &QWidget::close);

    win->show();
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::editSequences()
{
    // Обновим кэш, чтобы включить все последовательности из папок шаблонов
    refreshAllSequences();

    // 1. Проверяем, что есть наборы
    if (m_mw->m_seqMgr.sequences().isEmpty()) {
        QMessageBox::information(m_mw, QObject::tr("Нет наборов"),
                                 QObject::tr("В папке нет ни одного *.seq.json"));
        return;
    }

    // 2. Формируем список названий файлов (без пути)
    QStringList names;
    QMap<QString, QString> pathByName;      // имя → путь
    for (const QString &filePath : m_mw->m_seqMgr.fileList()) {
        QString name = QFileInfo(filePath).fileName();
        names << name;
        pathByName.insert(name, filePath);
    }

    // 3. Попросим пользователя выбрать файл
    bool ok = false;
    QString selectedName = QInputDialog::getItem(
                m_mw, QObject::tr("Выберите последовательность"),
                QObject::tr("Файл:"), names, 0, false, &ok);

    if (!ok || selectedName.isEmpty())
        return;          // пользователь отменил

    QString filePath = pathByName.value(selectedName);

    // 4. Загружаем выбранную последовательность
    PromptSequence seq = m_mw->m_seqMgr.sequences().value(filePath);

    openSequenceEditorWindow(QObject::tr("Редактор последовательности") + ": " + selectedName, filePath, seq);
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::createNewSequence()
{
    // Ask user for name
    bool ok=false;
    QString name = QInputDialog::getText(m_mw, "New sequence", "Name:", QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // Ask user whether to create in PromptSequences folder or in a template folder
    QMessageBox msgBar;
    msgBar.setWindowTitle("Create Sequence");
    msgBar.setText("Where would you like to create the sequence?");
    QAbstractButton* promptSeqButton = msgBar.addButton("In PromptSequences folder", QMessageBox::ActionRole);
    QAbstractButton* templateButton = msgBar.addButton("In Template folder", QMessageBox::ActionRole);
    QAbstractButton* cancelButton = msgBar.addButton(QMessageBox::Cancel);
    msgBar.exec();

    if (msgBar.clickedButton() == cancelButton) {
        return;
    } else if (msgBar.clickedButton() == promptSeqButton) {
        // Determine directory to save in PromptSequences
        QString dir;
        if (!m_mw->m_seqMgr.fileList().isEmpty()) {
            dir = QFileInfo(m_mw->m_seqMgr.fileList().first()).absolutePath();
        } else {
            dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                    .append("/DevelopersAssistant/PromptSequences");
            QDir().mkpath(dir);
            m_mw->m_seqMgr.setRoot(dir);
        }
        QString filePath = QDir(dir).filePath(name.trimmed() + ".seq.json");

        // Create default template and save
        PromptSequence seq = defaultSequenceTemplate();
        seq.setName(name.trimmed());
        if (!m_mw->m_seqMgr.save(seq, filePath)) {
            QMessageBox::warning(m_mw, "Error", "Cannot save sequence to: " + filePath);
            return;
        }
        refreshAllSequences();
        ensureDefaultSequence();
        QMessageBox::information(m_mw, "OK", "Sequence created:\n" + filePath);
    } else if (msgBar.clickedButton() == templateButton) {
        // Create in template folder - user needs to select a template directory
        QString templateDir = QFileDialog::getExistingDirectory(m_mw, "Select Template Folder",
                                                              QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        if (templateDir.isEmpty()) return;

        // Verify that the selected directory is a valid template directory by checking for template.templ.json
        QString templateJsonPath = QDir(templateDir).filePath("template.templ.json");
        if (!QFile::exists(templateJsonPath)) {
            QMessageBox::warning(m_mw, "Invalid Template Directory",
                               "The selected directory is not a valid template directory (missing template.templ.json).");
            return;
        }

        // Use the SolutionTemplateManager to create the new sequence in the template folder
        // This will create a default sequence structure in the template directory
        if (!m_mw->m_templateMgr.createNewSequenceInTemplate(templateDir, name.trimmed())) {
            QMessageBox::warning(m_mw, "Error", "Cannot create sequence in template directory: " + templateDir);
            return;
        }

        // Now we need to refresh both sequence and template managers to recognize the new sequence
        m_mw->m_templateMgr.refresh();
        refreshAllSequences(); // This will refresh both the main sequences and those in templates
        QMessageBox::information(m_mw, "OK", "Sequence created in template:\n" + templateDir + "/" + name.trimmed() + ".seq.json");
    }
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::refreshAllSequences()
{
    qDebug() << "[refreshAllSequences] Starting refresh of all sequences";

    // First, refresh the main sequence manager to get base sequences
    m_mw->m_seqMgr.refresh();
    QStringList allSequencePaths = m_mw->m_seqMgr.fileList();
    qDebug() << "[refreshAllSequences] Main sequence manager refreshed, base file count:" << allSequencePaths.size();

    // Then scan template folders for additional sequences and load them into the sequence manager
    const QStringList& templateDirs = m_mw->m_templateMgr.templateDirs();
    qDebug() << "[refreshAllSequences] Processing" << templateDirs.size() << "template directories";

    for (const QString& templateDir : templateDirs) {
        QDir dir(templateDir);
        if (dir.exists()) {
            // ищем *.seq.json в папке шаблона
            QStringList filters = {"*.seq.json"};
            QFileInfoList files = dir.entryInfoList(filters,
                                                    QDir::Files | QDir::NoSymLinks,
                                                    QDir::Name);
            qDebug() << "[refreshAllSequences] Found" << files.size() << "sequence files in" << templateDir;

            for (const QFileInfo& fileInfo : files) {
                QString path = fileInfo.absoluteFilePath();

                // Check if this sequence is already loaded to avoid duplicates
                if (!allSequencePaths.contains(path)) {
                    // Load the sequence from the file and add it to the sequence manager
                    QFile seqFile(path);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            PromptSequence seq = PromptSequence::fromJson(doc.object());
                            // Add to sequence manager cache using the addSequenceToCache method
                            m_mw->m_seqMgr.addSequenceToCache(path, seq);
                            qDebug() << "[refreshAllSequences] Loaded and added sequence from template:" << path;
                        }
                        seqFile.close();
                    }
                } else {
                    qDebug() << "[refreshAllSequences] Sequence already in manager, skipping:" << path;
                }
            }
        }
    }

    // Update the allSequencePaths list to reflect the current state after adding template sequences
    allSequencePaths = m_mw->m_seqMgr.fileList();
    qDebug() << "[refreshAllSequences] Completed discovery and loading, total sequence files in manager:" << allSequencePaths.size();

    // Add comprehensive logging about all available sequences for debugging
    const QMap<QString, PromptSequence>& allSequences = m_mw->m_seqMgr.sequences();
    qDebug() << "[refreshAllSequences] Detailed sequence list for debugging:";
    m_mw->log("=== SEQUENCE REFRESH DEBUG INFO ===");
    m_mw->log("Total sequences discovered: " + QString::number(allSequences.size()));
    for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
        QString seqName = it.value().name();
        QString filePath = it.key();
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();
        m_mw->log("  - Sequence: '" + seqName + "' -> File: '" + fileName + "' at " + filePath);
    }
    m_mw->log("=== END SEQUENCE DEBUG INFO ===");
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::ensureDefaultSequence()
{
    // Путь к файлу default.seq.json
    QString defaultFile = QDir(m_mw->m_seqMgr.root()).filePath("default.seq.json");

    if (QFile::exists(defaultFile))
        return;        // уже есть – ничего не делаем

    // Создаём объект-шаблон
    PromptSequence seq = defaultSequenceTemplate();

    // Сохраняем
    if (!m_mw->m_seqMgr.save(seq, defaultFile))
        qWarning() << "Не удалось создать файл default.seq.json";
}

/* ────────────────────────────────────────────────────── */
PromptSequence SequenceEditorController::defaultSequenceTemplate()
{
    // Step 1: Clarify the task if needed
    PromptStep s1;
    s1.setTemplate(
        "Ты — ИИ-помощник по рефакторингу и правкам кода.\n"
        "Контекст проекта:\n{{PROJECT_ROOT}}"
        "Вот список файлов:\n{{FILE_LIST}}\n"
        "\n{{HISTORY}}\n"
        "Если задача пользователя не до конца ясна — задай до 3 уточняющих вопросов, используя команду ASK_USER (один JSON на вопрос)."
        "При необходимости можешь сразу вызывать SEARCH_PROJECT (поиск по проекту) или SEARCH_WEB (внешние источники), прежде чем формировать план."
        "Доступные команды: {{SUPPORTED_COMMANDS}}"
        "Затем опиши краткий план действий."
    );
    s1.setAllowedCommands(QStringList{ "ASK_USER", "OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB" });

    // Step 2: Gather minimal relevant context (snippets only)
    PromptStep s2;
    s2.setTemplate(
        "На основе запроса и плана выбери минимальные необходимые участки кода."
        "Используй OPEN_FILE_REGION (желательно c startLine/endLine или регуляркой) для точечных фрагментов."
        "Если нужен целый файл (редко) — используй OPEN_FILE."
        "Для полнотекстового поиска по проекту используй SEARCH_PROJECT (укажи maxMatches и, при необходимости, include)."
        "Если требуются внешние сведения — вызови SEARCH_WEB и кратко перескажи найденные материалы."
        "Выведи только команды."
    );
    s2.setAllowedCommands(QStringList{ "OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB" });

    // Step 3: Propose concrete edits as commands (dry‑run)
    PromptStep s3;
    s3.setTemplate(
        "Сформируй набор правок."
        "• Для замены — REPLACE_IN_FILE"
        "• Для правки диапазона — EDIT_IN_RANGE"
        "• Для создания — CREATE_FILE"
        "• Для переименования/удаления — RENAME_FILE/DELETE_FILE"
        "Покажи только список JSON-команд без пояснений.\n"
        "Контекст:\n{{OPEN_FILES}}\n"
    );
    s3.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE" });

    // Step 4: If compilation hints are needed, ask short questions (optional)
    PromptStep s4;
    s4.setTemplate(
        "Если для точности правок не хватает 1-2 деталей — задай вопросы через ASK_USER, иначе верни пустой список."
    );
    s4.setAllowedCommands(QStringList{ "ASK_USER" });

    // Step 5: Finalize edits
    PromptStep s5;
    s5.setTemplate(
        "Выведи окончательные команды для применения изменений (повтори весь набор без пояснений)."
        "Если нужно показать пользователю результат или инструкцию — добавь команду SHOW_RESULT с title/content."
    );
    s5.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE", "SHOW_RESULT" });

    PromptSequence seq("Default");
    seq.setDescription("Поэтапная последовательность правок: уточнение → сбор минимального контекста → черновик правок → уточнения → финальные команды.");
    seq.addStep(s1);
    seq.addStep(s2);
    seq.addStep(s3);
    seq.addStep(s4);
    seq.addStep(s5);
    return seq;
}
