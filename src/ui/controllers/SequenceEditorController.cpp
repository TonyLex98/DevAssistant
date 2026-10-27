#include "SequenceEditorController.h"
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
#include "../../codeviewer/SequenceGraphWidget.h"
#include "../../ui/customWidgets/StyledButton.h"
#include "utils/Logging.h"

using namespace std::literals;

SequenceEditorController::SequenceEditorController(ISequenceService* seq, ITemplateService* tmpl, IGraphService* graph, IUIUpdater* ui)
    : m_seq(seq), m_tmpl(tmpl), m_graph(graph), m_ui(ui)
{
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::openSequenceGraph()
{
    qCDebug(logSeq) << "[MainWindow] openSequenceGraph called";

    // Refresh managers to ensure we have the latest data
    m_tmpl->templateMgrPtr()->refresh();
    refreshAllSequences();
    ensureDefaultSequence();

    auto* graphWin = m_graph->sequenceGraphWindow();
    if (graphWin) {
        graphWin->setSequenceManager(m_seq->seqMgrPtr());
        graphWin->setTemplateManager(m_tmpl->templateMgrPtr());
        graphWin->rebuildGraph();
        graphWin->setFocus();
        graphWin->setVisible(true);
        updateSequenceGraphTemplateSelection();
    }
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::onChatTemplateSelectionChanged(const QString &templatePath)
{
    QString normalizedPath = templatePath;
    if (normalizedPath == "simple_chat")
        normalizedPath.clear();

    if (m_seq->currentTemplatePath() == normalizedPath)
        return;

    m_seq->setCurrentTemplatePath(normalizedPath);
    updateSequenceGraphTemplateSelection();
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::updateSequenceGraphHighlight()
{
    auto* graphWin = m_graph->sequenceGraphWindow();
    if (graphWin && m_seq->isSequenceMode()) {
        graphWin->setCurrentSequence(m_seq->currentSeq().name());
    }
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::updateSequenceGraphTemplateSelection()
{
    auto* graphWin = m_graph->sequenceGraphWindow();
    if (graphWin) {
        graphWin->setTemplateSelectionByPath(m_seq->currentTemplatePath());
    }
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::openSequenceEditorForName(const QString &sequenceName)
{
    // Обновим кэш, чтобы включить все последовательности из папок шаблонов
    refreshAllSequences();

    // Find the sequence by name using the new method
    m_ui->log(QStringLiteral("DEBUG: Searching for sequence name: ") + sequenceName + QStringLiteral(" in openSequenceEditorForName"));
    QPair<QString, PromptSequence> result = m_seq->seqMgrPtr()->findSequenceByName(sequenceName);
    QString filePath = result.first;
    PromptSequence seq = result.second;

    if (filePath.isEmpty()) {
        // If not found in main manager, try to find in template directories
        m_ui->log(QStringLiteral("DEBUG: Sequence NOT FOUND in main manager, searching in template directories..."));

        // Search in template directories
        const QStringList& templateDirs = m_tmpl->templateMgrPtr()->templateDirs();
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
                                m_ui->log(QStringLiteral("DEBUG: Found sequence '") + sequenceName + QStringLiteral("' in template directory: ") + path);
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
        m_ui->log(QStringLiteral("DEBUG: Sequence NOT FOUND in openSequenceEditorForName. Available sequences:"));
        const QMap<QString, PromptSequence>& allSequences_debug = m_seq->seqMgrPtr()->sequences();
        for (auto it = allSequences_debug.constBegin(); it != allSequences_debug.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.fileName();
            m_ui->log(QStringLiteral("  - Available sequence name: '") + seqName + QStringLiteral("' from file: '") + fileName + QLatin1String("'"));
        }
        // Also list template directory contents
        const QStringList& templateDirs = m_tmpl->templateMgrPtr()->templateDirs();
        m_ui->log(QStringLiteral("  Template directories and their sequences:"));
        for (const QString& templateDir : templateDirs) {
            QDir dir(templateDir);
            if (dir.exists()) {
                QStringList filters = {"*.seq.json"};
                QFileInfoList files = dir.entryInfoList(filters,
                                                        QDir::Files | QDir::NoSymLinks,
                                                        QDir::Name);
                m_ui->log(QStringLiteral("    - Template directory: ") + templateDir);
                for (const QFileInfo& fileInfo : files) {
                    // Load and get the sequence name
                    QFile seqFile(fileInfo.absoluteFilePath());
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            PromptSequence tempSeq = PromptSequence::fromJson(doc.object());
                            m_ui->log(QStringLiteral("      - Sequence file: '") + fileInfo.fileName() + QStringLiteral("' with internal name: '") + tempSeq.name() + QLatin1String("'"));
                        }
                        seqFile.close();
                    }
                }
            }
        }
        QMessageBox::warning(nullptr, QStringLiteral("Error"), QStringLiteral("Sequence not found: ") + sequenceName);
        return;
    } else {
        m_ui->log(QStringLiteral("DEBUG: Successfully found sequence '") + sequenceName + QStringLiteral("' at path: ") + filePath);
    }

    openSequenceEditorWindow(QStringLiteral("Редактор последовательности") + ": " + sequenceName, filePath, seq);
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::openSequenceEditorWindow(const QString& title, const QString& filePath, const PromptSequence& seq)
{
    auto *win = new QWidget(nullptr);
    win->setWindowFlag(Qt::Window, true);
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->setWindowTitle(title);
    win->resize(1280, 820);
    win->setMinimumSize(1024, 700);

    auto *lay = new QVBoxLayout(win);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(10);

    auto *editor = new PromptSequenceEditor(win);
    editor->setSequenceManager(m_seq->seqMgrPtr());
    editor->setSequence(seq);
    editor->setCurrentSequencePath(filePath);
    lay->addWidget(editor, 1);

    auto *btnRow = new QHBoxLayout();
    auto *saveBtn = new StyledButton(QStringLiteral("Сохранить"), win);
    auto *closeBtn = new StyledButton(QStringLiteral("Закрыть"), win);
    saveBtn->setEnabled(false);
    btnRow->addStretch(1);
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);

    auto refreshAfterSave = [this]() {
        refreshAllSequences();
        ensureDefaultSequence();
        auto* graphWin = m_graph->sequenceGraphWindow();
        if (graphWin) {
            if (m_seq->currentTemplatePath().isEmpty()) {
                graphWin->rebuildGraph();
            } else {
                graphWin->rebuildGraphForTemplate(m_seq->currentTemplatePath());
                graphWin->setTemplateSelectionByPath(m_seq->currentTemplatePath());
            }
        }
    };

    QObject::connect(editor, &PromptSequenceEditor::sequenceChanged, win, [win, saveBtn](const PromptSequence& edited) {
        saveBtn->setEnabled(true);
        win->setWindowTitle(QStringLiteral("Редактор последовательности") + ": " + edited.name());
    });

    QObject::connect(saveBtn, &QPushButton::clicked, win, [this, editor, filePath, saveBtn, refreshAfterSave, win]() {
        const PromptSequence edited = editor->sequence();
        if (m_seq->seqMgrPtr()->save(edited, filePath)) {
            QMessageBox::information(win, QStringLiteral("Сохранено"), QStringLiteral("Последовательность сохранена."));
            saveBtn->setEnabled(false);
            refreshAfterSave();
        } else {
            QMessageBox::warning(win, QStringLiteral("Ошибка"), QStringLiteral("Не удалось сохранить."));
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
    if (m_seq->seqMgrPtr()->sequences().isEmpty()) {
        QMessageBox::information(nullptr, QStringLiteral("Нет наборов"),
                                 QStringLiteral("В папке нет ни одного *.seq.json"));
        return;
    }

    // 2. Формируем список названий файлов (без пути)
    QStringList names;
    QMap<QString, QString> pathByName;      // имя → путь
    for (const QString &filePath : m_seq->seqMgrPtr()->fileList()) {
        QString name = QFileInfo(filePath).fileName();
        names << name;
        pathByName.insert(name, filePath);
    }

    // 3. Попросим пользователя выбрать файл
    bool ok = false;
    QString selectedName = QInputDialog::getItem(
                nullptr, QStringLiteral("Выберите последовательность"),
                QStringLiteral("Файл:"), names, 0, false, &ok);

    if (!ok || selectedName.isEmpty())
        return;          // пользователь отменил

    QString filePath = pathByName.value(selectedName);

    // 4. Загружаем выбранную последовательность
    PromptSequence seq = m_seq->seqMgrPtr()->sequences().value(filePath);

    openSequenceEditorWindow(QStringLiteral("Редактор последовательности") + ": " + selectedName, filePath, seq);
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::createNewSequence()
{
    // Ask user for name
    bool ok=false;
    QString name = QInputDialog::getText(nullptr, QStringLiteral("New sequence"), QStringLiteral("Name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // Ask user whether to create in PromptSequences folder or in a template folder
    QMessageBox msgBar;
    msgBar.setWindowTitle(QStringLiteral("Create Sequence"));
    msgBar.setText(QStringLiteral("Where would you like to create the sequence?"));
    QAbstractButton* promptSeqButton = msgBar.addButton(QStringLiteral("In PromptSequences folder"), QMessageBox::ActionRole);
    QAbstractButton* templateButton = msgBar.addButton(QStringLiteral("In Template folder"), QMessageBox::ActionRole);
    QAbstractButton* cancelButton = msgBar.addButton(QMessageBox::Cancel);
    msgBar.exec();

    if (msgBar.clickedButton() == cancelButton) {
        return;
    } else if (msgBar.clickedButton() == promptSeqButton) {
        // Determine directory to save in PromptSequences
        QString dir;
        if (!m_seq->seqMgrPtr()->fileList().isEmpty()) {
            dir = QFileInfo(m_seq->seqMgrPtr()->fileList().first()).absolutePath();
        } else {
            dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                    .append("/DevelopersAssistant/PromptSequences");
            QDir().mkpath(dir);
            m_seq->seqMgrPtr()->setRoot(dir);
        }
        QString filePath = QDir(dir).filePath(name.trimmed() + ".seq.json");

        // Create default template and save
        PromptSequence seq = defaultSequenceTemplate();
        seq.setName(name.trimmed());
        if (!m_seq->seqMgrPtr()->save(seq, filePath)) {
            QMessageBox::warning(nullptr, QStringLiteral("Error"), QStringLiteral("Cannot save sequence to: ") + filePath);
            return;
        }
        refreshAllSequences();
        ensureDefaultSequence();
        QMessageBox::information(nullptr, QStringLiteral("OK"), QStringLiteral("Sequence created:\n") + filePath);
    } else if (msgBar.clickedButton() == templateButton) {
        // Create in template folder - user needs to select a template directory
        QString templateDir = QFileDialog::getExistingDirectory(nullptr, QStringLiteral("Select Template Folder"),
                                                              QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        if (templateDir.isEmpty()) return;

        // Verify that the selected directory is a valid template directory by checking for template.templ.json
        QString templateJsonPath = QDir(templateDir).filePath("template.templ.json");
        if (!QFile::exists(templateJsonPath)) {
            QMessageBox::warning(nullptr, QStringLiteral("Invalid Template Directory"),
                               QStringLiteral("The selected directory is not a valid template directory (missing template.templ.json)."));
            return;
        }

        // Use the SolutionTemplateManager to create the new sequence in the template folder
        // This will create a default sequence structure in the template directory
        if (!m_tmpl->templateMgrPtr()->createNewSequenceInTemplate(templateDir, name.trimmed())) {
            QMessageBox::warning(nullptr, QStringLiteral("Error"), QStringLiteral("Cannot create sequence in template directory: ") + templateDir);
            return;
        }

        // Now we need to refresh both sequence and template managers to recognize the new sequence
        m_tmpl->templateMgrPtr()->refresh();
        refreshAllSequences(); // This will refresh both the main sequences and those in templates
        QMessageBox::information(nullptr, QStringLiteral("OK"), QStringLiteral("Sequence created in template:\n") + templateDir + "/" + name.trimmed() + ".seq.json");
    }
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::refreshAllSequences()
{
    qCDebug(logSeq) << "[refreshAllSequences] Starting refresh of all sequences";

    // First, refresh the main sequence manager to get base sequences
    m_seq->seqMgrPtr()->refresh();
    QStringList allSequencePaths = m_seq->seqMgrPtr()->fileList();
    qCDebug(logSeq) << "[refreshAllSequences] Main sequence manager refreshed, base file count:" << allSequencePaths.size();

    // Then scan template folders for additional sequences and load them into the sequence manager
    const QStringList& templateDirs = m_tmpl->templateMgrPtr()->templateDirs();
    qCDebug(logSeq) << "[refreshAllSequences] Processing" << templateDirs.size() << "template directories";

    for (const QString& templateDir : templateDirs) {
        QDir dir(templateDir);
        if (dir.exists()) {
            // ищем *.seq.json в папке шаблона
            QStringList filters = {"*.seq.json"};
            QFileInfoList files = dir.entryInfoList(filters,
                                                    QDir::Files | QDir::NoSymLinks,
                                                    QDir::Name);
            qCDebug(logSeq) << "[refreshAllSequences] Found" << files.size() << "sequence files in" << templateDir;

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
                            m_seq->seqMgrPtr()->addSequenceToCache(path, seq);
                            qCDebug(logSeq) << "[refreshAllSequences] Loaded and added sequence from template:" << path;
                        }
                        seqFile.close();
                    }
                } else {
                    qCDebug(logSeq) << "[refreshAllSequences] Sequence already in manager, skipping:" << path;
                }
            }
        }
    }

    // Update the allSequencePaths list to reflect the current state after adding template sequences
    allSequencePaths = m_seq->seqMgrPtr()->fileList();
    qCDebug(logSeq) << "[refreshAllSequences] Completed discovery and loading, total sequence files in manager:" << allSequencePaths.size();

    // Add comprehensive logging about all available sequences for debugging
    const QMap<QString, PromptSequence>& allSequences = m_seq->seqMgrPtr()->sequences();
    qCDebug(logSeq) << "[refreshAllSequences] Detailed sequence list for debugging:";
    m_ui->log(QStringLiteral("=== SEQUENCE REFRESH DEBUG INFO ==="));
    m_ui->log(QStringLiteral("Total sequences discovered: ") + QString::number(allSequences.size()));
    for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
        QString seqName = it.value().name();
        QString filePath = it.key();
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();
        m_ui->log(QStringLiteral("  - Sequence: '") + seqName + QStringLiteral("' -> File: '") + fileName + QStringLiteral("' at ") + filePath);
    }
    m_ui->log(QStringLiteral("=== END SEQUENCE DEBUG INFO ==="));
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::ensureDefaultSequence()
{
    // Путь к файлу default.seq.json
    QString defaultFile = QDir(m_seq->seqMgrPtr()->root()).filePath("default.seq.json");

    if (QFile::exists(defaultFile))
        return;        // уже есть – ничего не делаем

    // Создаём объект-шаблон
    PromptSequence seq = defaultSequenceTemplate();

    // Сохраняем
    if (!m_seq->seqMgrPtr()->save(seq, defaultFile))
        qCWarning(logSeq) << "Не удалось создать файл default.seq.json";
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
