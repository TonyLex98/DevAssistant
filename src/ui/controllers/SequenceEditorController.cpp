#include "SequenceEditorController.h"
#include <QCoreApplication>
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
    // Refresh the cache so it includes the sequences from template folders.
    refreshAllSequences();

    qCDebug(logSeq) << "[openSequenceEditorForName] Searching for sequence name:" << sequenceName;
    QPair<QString, PromptSequence> result = m_seq->seqMgrPtr()->findSequenceByName(sequenceName);
    QString filePath = result.first;
    PromptSequence seq = result.second;

    if (filePath.isEmpty()) {
        // Not in the main manager: look in the template directories.
        qCDebug(logSeq) << "[openSequenceEditorForName] Not found in the main manager, searching template directories";

        const QStringList& templateDirs = m_tmpl->templateMgrPtr()->templateDirs();
        for (const QString& templateDir : templateDirs) {
            QDir dir(templateDir);
            if (dir.exists()) {
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
                                qCDebug(logSeq) << "[openSequenceEditorForName] Found" << sequenceName << "in template directory:" << path;
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
        qCDebug(logSeq) << "[openSequenceEditorForName] Sequence not found:" << sequenceName;
        const QMap<QString, PromptSequence>& allSequences_debug = m_seq->seqMgrPtr()->sequences();
        for (auto it = allSequences_debug.constBegin(); it != allSequences_debug.constEnd(); ++it)
            qCDebug(logSeq) << "  available:" << it.value().name() << "file:" << QFileInfo(it.key()).fileName();
        QMessageBox::warning(nullptr, QCoreApplication::translate("SequenceEditorController", "Error"),
                             QCoreApplication::translate("SequenceEditorController", "Sequence not found: %1").arg(sequenceName));
        return;
    } else {
        qCDebug(logSeq) << "[openSequenceEditorForName] Found" << sequenceName << "at" << filePath;
    }

    openSequenceEditorWindow(QCoreApplication::translate("SequenceEditorController", "Sequence editor: %1").arg(sequenceName),
                             filePath, seq);
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
    auto *saveBtn = new StyledButton(QCoreApplication::translate("SequenceEditorController", "Save"), win);
    auto *closeBtn = new StyledButton(QCoreApplication::translate("SequenceEditorController", "Close"), win);
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
        win->setWindowTitle(QCoreApplication::translate("SequenceEditorController", "Sequence editor: %1").arg(edited.name()));
    });

    QObject::connect(saveBtn, &QPushButton::clicked, win, [this, editor, filePath, saveBtn, refreshAfterSave, win]() {
        const PromptSequence edited = editor->sequence();
        if (m_seq->seqMgrPtr()->save(edited, filePath)) {
            QMessageBox::information(win, QCoreApplication::translate("SequenceEditorController", "Saved"),
                                     QCoreApplication::translate("SequenceEditorController", "The sequence has been saved."));
            saveBtn->setEnabled(false);
            refreshAfterSave();
        } else {
            QMessageBox::warning(win, QCoreApplication::translate("SequenceEditorController", "Error"),
                                 QCoreApplication::translate("SequenceEditorController", "Could not save the sequence."));
        }
    });

    QObject::connect(closeBtn, &QPushButton::clicked, win, &QWidget::close);

    win->show();
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::editSequences()
{
    // Refresh the cache so it includes the sequences from template folders.
    refreshAllSequences();

    if (m_seq->seqMgrPtr()->sequences().isEmpty()) {
        QMessageBox::information(nullptr, QCoreApplication::translate("SequenceEditorController", "No sequences"),
                                 QCoreApplication::translate("SequenceEditorController", "The folder has no *.seq.json files."));
        return;
    }

    // File names without the path, mapped to the full path.
    QStringList names;
    QMap<QString, QString> pathByName;
    for (const QString &filePath : m_seq->seqMgrPtr()->fileList()) {
        QString name = QFileInfo(filePath).fileName();
        names << name;
        pathByName.insert(name, filePath);
    }

    bool ok = false;
    QString selectedName = QInputDialog::getItem(
                nullptr, QCoreApplication::translate("SequenceEditorController", "Choose a sequence"),
                QCoreApplication::translate("SequenceEditorController", "File:"), names, 0, false, &ok);

    if (!ok || selectedName.isEmpty())
        return;

    QString filePath = pathByName.value(selectedName);
    PromptSequence seq = m_seq->seqMgrPtr()->sequences().value(filePath);

    openSequenceEditorWindow(QCoreApplication::translate("SequenceEditorController", "Sequence editor: %1").arg(selectedName),
                             filePath, seq);
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::createNewSequence()
{
    // Ask user for name
    bool ok=false;
    QString name = QInputDialog::getText(nullptr, QCoreApplication::translate("SequenceEditorController", "New sequence"),
                                         QCoreApplication::translate("SequenceEditorController", "Name:"),
                                         QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // Ask user whether to create in PromptSequences folder or in a template folder
    QMessageBox msgBar;
    msgBar.setWindowTitle(QCoreApplication::translate("SequenceEditorController", "Create Sequence"));
    msgBar.setText(QCoreApplication::translate("SequenceEditorController", "Where would you like to create the sequence?"));
    QAbstractButton* promptSeqButton = msgBar.addButton(
        QCoreApplication::translate("SequenceEditorController", "In PromptSequences folder"), QMessageBox::ActionRole);
    QAbstractButton* templateButton = msgBar.addButton(
        QCoreApplication::translate("SequenceEditorController", "In Template folder"), QMessageBox::ActionRole);
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
            QMessageBox::warning(nullptr, QCoreApplication::translate("SequenceEditorController", "Error"),
                                 QCoreApplication::translate("SequenceEditorController", "Cannot save sequence to: %1").arg(filePath));
            return;
        }
        refreshAllSequences();
        ensureDefaultSequence();
        QMessageBox::information(nullptr, QCoreApplication::translate("SequenceEditorController", "OK"),
                                 QCoreApplication::translate("SequenceEditorController", "Sequence created:\n%1").arg(filePath));
    } else if (msgBar.clickedButton() == templateButton) {
        // Create in template folder - user needs to select a template directory
        QString templateDir = QFileDialog::getExistingDirectory(nullptr,
                                                                QCoreApplication::translate("SequenceEditorController", "Select Template Folder"),
                                                                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        if (templateDir.isEmpty()) return;

        // Verify that the selected directory is a valid template directory by checking for template.templ.json
        QString templateJsonPath = QDir(templateDir).filePath("template.templ.json");
        if (!QFile::exists(templateJsonPath)) {
            QMessageBox::warning(nullptr, QCoreApplication::translate("SequenceEditorController", "Invalid Template Directory"),
                                 QCoreApplication::translate("SequenceEditorController",
                                     "The selected directory is not a valid template directory (missing template.templ.json)."));
            return;
        }

        // Use the SolutionTemplateManager to create the new sequence in the template folder
        // This will create a default sequence structure in the template directory
        if (!m_tmpl->templateMgrPtr()->createNewSequenceInTemplate(templateDir, name.trimmed())) {
            QMessageBox::warning(nullptr, QCoreApplication::translate("SequenceEditorController", "Error"),
                                 QCoreApplication::translate("SequenceEditorController",
                                     "Cannot create sequence in template directory: %1").arg(templateDir));
            return;
        }

        // Now we need to refresh both sequence and template managers to recognize the new sequence
        m_tmpl->templateMgrPtr()->refresh();
        refreshAllSequences(); // This will refresh both the main sequences and those in templates
        QMessageBox::information(nullptr, QCoreApplication::translate("SequenceEditorController", "OK"),
                                 QCoreApplication::translate("SequenceEditorController", "Sequence created in template:\n%1")
                                     .arg(templateDir + "/" + name.trimmed() + ".seq.json"));
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

    const QMap<QString, PromptSequence>& allSequences = m_seq->seqMgrPtr()->sequences();
    qCDebug(logSeq) << "[refreshAllSequences] Sequences discovered:" << allSequences.size();
    for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it)
        qCDebug(logSeq) << "  sequence:" << it.value().name() << "file:" << it.key();
}

/* ────────────────────────────────────────────────────── */
void SequenceEditorController::ensureDefaultSequence()
{
    QString defaultFile = QDir(m_seq->seqMgrPtr()->root()).filePath("default.seq.json");

    if (QFile::exists(defaultFile))
        return;

    PromptSequence seq = defaultSequenceTemplate();

    if (!m_seq->seqMgrPtr()->save(seq, defaultFile))
        qCWarning(logSeq) << "Could not create default.seq.json";
}

/* ────────────────────────────────────────────────────── */
PromptSequence SequenceEditorController::defaultSequenceTemplate()
{
    // Step 1: Clarify the task if needed
    PromptStep s1;
    s1.setTemplate(
        "You are an AI assistant for refactoring and editing code.\n"
        "Project context:\n{{PROJECT_ROOT}}\n"
        "Files:\n{{FILE_LIST}}\n"
        "\n{{HISTORY}}\n"
        "If the user's task is not entirely clear, ask up to 3 clarifying questions with the ASK_USER command (one JSON object per question).\n"
        "If needed, call SEARCH_PROJECT (search the project) or SEARCH_WEB (external sources) before writing the plan.\n"
        "Available commands: {{SUPPORTED_COMMANDS}}\n"
        "Then describe a short plan of action."
    );
    s1.setAllowedCommands(QStringList{ "ASK_USER", "OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB" });

    // Step 2: Gather minimal relevant context (snippets only)
    PromptStep s2;
    s2.setTemplate(
        "Based on the request and the plan, pick the smallest pieces of code you need.\n"
        "Use OPEN_FILE_REGION (preferably with startLine/endLine or a regular expression) for specific fragments.\n"
        "If you need a whole file (rarely), use OPEN_FILE.\n"
        "For full-text search in the project, use SEARCH_PROJECT (set maxMatches and, if needed, include).\n"
        "If you need outside information, call SEARCH_WEB and briefly summarize what you found.\n"
        "Output only commands."
    );
    s2.setAllowedCommands(QStringList{ "OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB" });

    // Step 3: Propose concrete edits as commands (dry‑run)
    PromptStep s3;
    s3.setTemplate(
        "Prepare the set of edits.\n"
        "• To replace text: REPLACE_IN_FILE\n"
        "• To edit a range of lines: EDIT_IN_RANGE\n"
        "• To create a file: CREATE_FILE\n"
        "• To rename or delete: RENAME_FILE/DELETE_FILE\n"
        "Output only the list of JSON commands, without explanations.\n"
        "Context:\n{{OPEN_FILES}}\n"
    );
    s3.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE" });

    // Step 4: If compilation hints are needed, ask short questions (optional)
    PromptStep s4;
    s4.setTemplate(
        "If 1-2 details are missing to make the edits precise, ask about them with ASK_USER; otherwise return an empty list."
    );
    s4.setAllowedCommands(QStringList{ "ASK_USER" });

    // Step 5: Finalize edits
    PromptStep s5;
    s5.setTemplate(
        "Output the final commands that apply the changes (repeat the whole set, without explanations).\n"
        "If the user should see a result or instructions, add a SHOW_RESULT command with title/content."
    );
    s5.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE", "SHOW_RESULT" });

    PromptSequence seq("Default");
    seq.setDescription("Step-by-step editing: clarify → collect minimal context → draft edits → follow-up questions → final commands.");
    seq.addStep(s1);
    seq.addStep(s2);
    seq.addStep(s3);
    seq.addStep(s4);
    seq.addStep(s5);
    return seq;
}
