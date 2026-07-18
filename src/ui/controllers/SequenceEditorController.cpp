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

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
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

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
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

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
void SequenceEditorController::updateSequenceGraphHighlight()
{
    auto* graphWin = m_graph->sequenceGraphWindow();
    if (graphWin && m_seq->isSequenceMode()) {
        graphWin->setCurrentSequence(m_seq->currentSeq().name());
    }
}

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
void SequenceEditorController::updateSequenceGraphTemplateSelection()
{
    auto* graphWin = m_graph->sequenceGraphWindow();
    if (graphWin) {
        graphWin->setTemplateSelectionByPath(m_seq->currentTemplatePath());
    }
}

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
void SequenceEditorController::openSequenceEditorForName(const QString &sequenceName)
{
    // РћР±РЅРѕРІРёРј РєСЌС€, С‡С‚РѕР±С‹ РІРєР»СЋС‡РёС‚СЊ РІСЃРµ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚Рё РёР· РїР°РїРѕРє С€Р°Р±Р»РѕРЅРѕРІ
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
                // РёС‰РµРј *.seq.json РІ РїР°РїРєРµ С€Р°Р±Р»РѕРЅР°
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

    openSequenceEditorWindow(QStringLiteral("Р РµРґР°РєС‚РѕСЂ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚Рё") + ": " + sequenceName, filePath, seq);
}

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
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
    auto *saveBtn = new StyledButton(QStringLiteral("РЎРѕС…СЂР°РЅРёС‚СЊ"), win);
    auto *closeBtn = new StyledButton(QStringLiteral("Р—Р°РєСЂС‹С‚СЊ"), win);
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
        win->setWindowTitle(QStringLiteral("Р РµРґР°РєС‚РѕСЂ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚Рё") + ": " + edited.name());
    });

    QObject::connect(saveBtn, &QPushButton::clicked, win, [this, editor, filePath, saveBtn, refreshAfterSave, win]() {
        const PromptSequence edited = editor->sequence();
        if (m_seq->seqMgrPtr()->save(edited, filePath)) {
            QMessageBox::information(win, QStringLiteral("РЎРѕС…СЂР°РЅРµРЅРѕ"), QStringLiteral("РџРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚СЊ СЃРѕС…СЂР°РЅРµРЅР°."));
            saveBtn->setEnabled(false);
            refreshAfterSave();
        } else {
            QMessageBox::warning(win, QStringLiteral("РћС€РёР±РєР°"), QStringLiteral("РќРµ СѓРґР°Р»РѕСЃСЊ СЃРѕС…СЂР°РЅРёС‚СЊ."));
        }
    });

    QObject::connect(closeBtn, &QPushButton::clicked, win, &QWidget::close);

    win->show();
}

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
void SequenceEditorController::editSequences()
{
    // РћР±РЅРѕРІРёРј РєСЌС€, С‡С‚РѕР±С‹ РІРєР»СЋС‡РёС‚СЊ РІСЃРµ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚Рё РёР· РїР°РїРѕРє С€Р°Р±Р»РѕРЅРѕРІ
    refreshAllSequences();

    // 1. РџСЂРѕРІРµСЂСЏРµРј, С‡С‚Рѕ РµСЃС‚СЊ РЅР°Р±РѕСЂС‹
    if (m_seq->seqMgrPtr()->sequences().isEmpty()) {
        QMessageBox::information(nullptr, QStringLiteral("РќРµС‚ РЅР°Р±РѕСЂРѕРІ"),
                                 QStringLiteral("Р’ РїР°РїРєРµ РЅРµС‚ РЅРё РѕРґРЅРѕРіРѕ *.seq.json"));
        return;
    }

    // 2. Р¤РѕСЂРјРёСЂСѓРµРј СЃРїРёСЃРѕРє РЅР°Р·РІР°РЅРёР№ С„Р°Р№Р»РѕРІ (Р±РµР· РїСѓС‚Рё)
    QStringList names;
    QMap<QString, QString> pathByName;      // РёРјСЏ в†’ РїСѓС‚СЊ
    for (const QString &filePath : m_seq->seqMgrPtr()->fileList()) {
        QString name = QFileInfo(filePath).fileName();
        names << name;
        pathByName.insert(name, filePath);
    }

    // 3. РџРѕРїСЂРѕСЃРёРј РїРѕР»СЊР·РѕРІР°С‚РµР»СЏ РІС‹Р±СЂР°С‚СЊ С„Р°Р№Р»
    bool ok = false;
    QString selectedName = QInputDialog::getItem(
                nullptr, QStringLiteral("Р’С‹Р±РµСЂРёС‚Рµ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚СЊ"),
                QStringLiteral("Р¤Р°Р№Р»:"), names, 0, false, &ok);

    if (!ok || selectedName.isEmpty())
        return;          // РїРѕР»СЊР·РѕРІР°С‚РµР»СЊ РѕС‚РјРµРЅРёР»

    QString filePath = pathByName.value(selectedName);

    // 4. Р—Р°РіСЂСѓР¶Р°РµРј РІС‹Р±СЂР°РЅРЅСѓСЋ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚СЊ
    PromptSequence seq = m_seq->seqMgrPtr()->sequences().value(filePath);

    openSequenceEditorWindow(QStringLiteral("Р РµРґР°РєС‚РѕСЂ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚Рё") + ": " + selectedName, filePath, seq);
}

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
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

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
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
            // РёС‰РµРј *.seq.json РІ РїР°РїРєРµ С€Р°Р±Р»РѕРЅР°
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

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
void SequenceEditorController::ensureDefaultSequence()
{
    // РџСѓС‚СЊ Рє С„Р°Р№Р»Сѓ default.seq.json
    QString defaultFile = QDir(m_seq->seqMgrPtr()->root()).filePath("default.seq.json");

    if (QFile::exists(defaultFile))
        return;        // СѓР¶Рµ РµСЃС‚СЊ вЂ“ РЅРёС‡РµРіРѕ РЅРµ РґРµР»Р°РµРј

    // РЎРѕР·РґР°С‘Рј РѕР±СЉРµРєС‚-С€Р°Р±Р»РѕРЅ
    PromptSequence seq = defaultSequenceTemplate();

    // РЎРѕС…СЂР°РЅСЏРµРј
    if (!m_seq->seqMgrPtr()->save(seq, defaultFile))
        qCWarning(logSeq) << "РќРµ СѓРґР°Р»РѕСЃСЊ СЃРѕР·РґР°С‚СЊ С„Р°Р№Р» default.seq.json";
}

/* в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ */
PromptSequence SequenceEditorController::defaultSequenceTemplate()
{
    // Step 1: Clarify the task if needed
    PromptStep s1;
    s1.setTemplate(
        "РўС‹ вЂ” РР-РїРѕРјРѕС‰РЅРёРє РїРѕ СЂРµС„Р°РєС‚РѕСЂРёРЅРіСѓ Рё РїСЂР°РІРєР°Рј РєРѕРґР°.\n"
        "РљРѕРЅС‚РµРєСЃС‚ РїСЂРѕРµРєС‚Р°:\n{{PROJECT_ROOT}}"
        "Р’РѕС‚ СЃРїРёСЃРѕРє С„Р°Р№Р»РѕРІ:\n{{FILE_LIST}}\n"
        "\n{{HISTORY}}\n"
        "Р•СЃР»Рё Р·Р°РґР°С‡Р° РїРѕР»СЊР·РѕРІР°С‚РµР»СЏ РЅРµ РґРѕ РєРѕРЅС†Р° СЏСЃРЅР° вЂ” Р·Р°РґР°Р№ РґРѕ 3 СѓС‚РѕС‡РЅСЏСЋС‰РёС… РІРѕРїСЂРѕСЃРѕРІ, РёСЃРїРѕР»СЊР·СѓСЏ РєРѕРјР°РЅРґСѓ ASK_USER (РѕРґРёРЅ JSON РЅР° РІРѕРїСЂРѕСЃ)."
        "РџСЂРё РЅРµРѕР±С…РѕРґРёРјРѕСЃС‚Рё РјРѕР¶РµС€СЊ СЃСЂР°Р·Сѓ РІС‹Р·С‹РІР°С‚СЊ SEARCH_PROJECT (РїРѕРёСЃРє РїРѕ РїСЂРѕРµРєС‚Сѓ) РёР»Рё SEARCH_WEB (РІРЅРµС€РЅРёРµ РёСЃС‚РѕС‡РЅРёРєРё), РїСЂРµР¶РґРµ С‡РµРј С„РѕСЂРјРёСЂРѕРІР°С‚СЊ РїР»Р°РЅ."
        "Р”РѕСЃС‚СѓРїРЅС‹Рµ РєРѕРјР°РЅРґС‹: {{SUPPORTED_COMMANDS}}"
        "Р—Р°С‚РµРј РѕРїРёС€Рё РєСЂР°С‚РєРёР№ РїР»Р°РЅ РґРµР№СЃС‚РІРёР№."
    );
    s1.setAllowedCommands(QStringList{ "ASK_USER", "OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB" });

    // Step 2: Gather minimal relevant context (snippets only)
    PromptStep s2;
    s2.setTemplate(
        "РќР° РѕСЃРЅРѕРІРµ Р·Р°РїСЂРѕСЃР° Рё РїР»Р°РЅР° РІС‹Р±РµСЂРё РјРёРЅРёРјР°Р»СЊРЅС‹Рµ РЅРµРѕР±С…РѕРґРёРјС‹Рµ СѓС‡Р°СЃС‚РєРё РєРѕРґР°."
        "РСЃРїРѕР»СЊР·СѓР№ OPEN_FILE_REGION (Р¶РµР»Р°С‚РµР»СЊРЅРѕ c startLine/endLine РёР»Рё СЂРµРіСѓР»СЏСЂРєРѕР№) РґР»СЏ С‚РѕС‡РµС‡РЅС‹С… С„СЂР°РіРјРµРЅС‚РѕРІ."
        "Р•СЃР»Рё РЅСѓР¶РµРЅ С†РµР»С‹Р№ С„Р°Р№Р» (СЂРµРґРєРѕ) вЂ” РёСЃРїРѕР»СЊР·СѓР№ OPEN_FILE."
        "Р”Р»СЏ РїРѕР»РЅРѕС‚РµРєСЃС‚РѕРІРѕРіРѕ РїРѕРёСЃРєР° РїРѕ РїСЂРѕРµРєС‚Сѓ РёСЃРїРѕР»СЊР·СѓР№ SEARCH_PROJECT (СѓРєР°Р¶Рё maxMatches Рё, РїСЂРё РЅРµРѕР±С…РѕРґРёРјРѕСЃС‚Рё, include)."
        "Р•СЃР»Рё С‚СЂРµР±СѓСЋС‚СЃСЏ РІРЅРµС€РЅРёРµ СЃРІРµРґРµРЅРёСЏ вЂ” РІС‹Р·РѕРІРё SEARCH_WEB Рё РєСЂР°С‚РєРѕ РїРµСЂРµСЃРєР°Р¶Рё РЅР°Р№РґРµРЅРЅС‹Рµ РјР°С‚РµСЂРёР°Р»С‹."
        "Р’С‹РІРµРґРё С‚РѕР»СЊРєРѕ РєРѕРјР°РЅРґС‹."
    );
    s2.setAllowedCommands(QStringList{ "OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB" });

    // Step 3: Propose concrete edits as commands (dryвЂ‘run)
    PromptStep s3;
    s3.setTemplate(
        "РЎС„РѕСЂРјРёСЂСѓР№ РЅР°Р±РѕСЂ РїСЂР°РІРѕРє."
        "вЂў Р”Р»СЏ Р·Р°РјРµРЅС‹ вЂ” REPLACE_IN_FILE"
        "вЂў Р”Р»СЏ РїСЂР°РІРєРё РґРёР°РїР°Р·РѕРЅР° вЂ” EDIT_IN_RANGE"
        "вЂў Р”Р»СЏ СЃРѕР·РґР°РЅРёСЏ вЂ” CREATE_FILE"
        "вЂў Р”Р»СЏ РїРµСЂРµРёРјРµРЅРѕРІР°РЅРёСЏ/СѓРґР°Р»РµРЅРёСЏ вЂ” RENAME_FILE/DELETE_FILE"
        "РџРѕРєР°Р¶Рё С‚РѕР»СЊРєРѕ СЃРїРёСЃРѕРє JSON-РєРѕРјР°РЅРґ Р±РµР· РїРѕСЏСЃРЅРµРЅРёР№.\n"
        "РљРѕРЅС‚РµРєСЃС‚:\n{{OPEN_FILES}}\n"
    );
    s3.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE" });

    // Step 4: If compilation hints are needed, ask short questions (optional)
    PromptStep s4;
    s4.setTemplate(
        "Р•СЃР»Рё РґР»СЏ С‚РѕС‡РЅРѕСЃС‚Рё РїСЂР°РІРѕРє РЅРµ С…РІР°С‚Р°РµС‚ 1-2 РґРµС‚Р°Р»РµР№ вЂ” Р·Р°РґР°Р№ РІРѕРїСЂРѕСЃС‹ С‡РµСЂРµР· ASK_USER, РёРЅР°С‡Рµ РІРµСЂРЅРё РїСѓСЃС‚РѕР№ СЃРїРёСЃРѕРє."
    );
    s4.setAllowedCommands(QStringList{ "ASK_USER" });

    // Step 5: Finalize edits
    PromptStep s5;
    s5.setTemplate(
        "Р’С‹РІРµРґРё РѕРєРѕРЅС‡Р°С‚РµР»СЊРЅС‹Рµ РєРѕРјР°РЅРґС‹ РґР»СЏ РїСЂРёРјРµРЅРµРЅРёСЏ РёР·РјРµРЅРµРЅРёР№ (РїРѕРІС‚РѕСЂРё РІРµСЃСЊ РЅР°Р±РѕСЂ Р±РµР· РїРѕСЏСЃРЅРµРЅРёР№)."
        "Р•СЃР»Рё РЅСѓР¶РЅРѕ РїРѕРєР°Р·Р°С‚СЊ РїРѕР»СЊР·РѕРІР°С‚РµР»СЋ СЂРµР·СѓР»СЊС‚Р°С‚ РёР»Рё РёРЅСЃС‚СЂСѓРєС†РёСЋ вЂ” РґРѕР±Р°РІСЊ РєРѕРјР°РЅРґСѓ SHOW_RESULT СЃ title/content."
    );
    s5.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE", "SHOW_RESULT" });

    PromptSequence seq("Default");
    seq.setDescription("РџРѕСЌС‚Р°РїРЅР°СЏ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚СЊ РїСЂР°РІРѕРє: СѓС‚РѕС‡РЅРµРЅРёРµ в†’ СЃР±РѕСЂ РјРёРЅРёРјР°Р»СЊРЅРѕРіРѕ РєРѕРЅС‚РµРєСЃС‚Р° в†’ С‡РµСЂРЅРѕРІРёРє РїСЂР°РІРѕРє в†’ СѓС‚РѕС‡РЅРµРЅРёСЏ в†’ С„РёРЅР°Р»СЊРЅС‹Рµ РєРѕРјР°РЅРґС‹.");
    seq.addStep(s1);
    seq.addStep(s2);
    seq.addStep(s3);
    seq.addStep(s4);
    seq.addStep(s5);
    return seq;
}
