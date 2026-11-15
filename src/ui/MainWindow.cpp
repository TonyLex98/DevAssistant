#include "ui/MainWindow.h"
#include "core/project/DescriptionGenerator.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSet>
#include <QDateTime>
#include <QSizePolicy>

#include <QStatusBar>
#include <QCoreApplication>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTime>
#include <QLabel>
#include <QFrame>
#include "ui/ChatDialog.h"
#include "ui/dialogs/OllamaSettingsDialog.h"
#include "core/util/AppConfig.h"
#include "ui/dialogs/PromptEditorDialog.h"
#include "ui/dialogs/ChangesDialog.h"
#include "ui/dialogs/RunTemplateOrSequenceDialog.h"
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTimer>
#include <QScrollArea>
#include <QStackedWidget>
#include <algorithm>
#include "core/commands/EditInRangeCommand.h"
#include "core/commands/RunShellCommand.h"
#include "core/commands/RunPythonCommand.h"
#include "core/commands/SearchWebCommand.h"
#include "core/commands/SearchProjectCommand.h"
#include "core/commands/ShowResultCommand.h"
#include "core/commands/Command.h"
#include "core/commands/CommandDocs.h"
#include "ui/dialogs/SolutionTemplateViewer.h"
#include "core/workflow/SolutionTemplateManager.h"
#include "core/util/json_utils.h"
#include "ui/controllers/UIWidgets.h"
#include "ui/controllers/UiBuilder.h"
#include "core/util/Logging.h"

static QVariantMap generationParamsFromConfig(const AppConfig& cfg)
{
    if (!cfg.params.isEmpty())
        return cfg.params;

    QVariantMap genParams;
    genParams["temperature"] = cfg.temperature;
    genParams["top_p"] = cfg.topP;
    genParams["top_k"] = cfg.topK;
    genParams["repeat_penalty"] = cfg.repeatPenalty;
    genParams["presence_penalty"] = cfg.presencePenalty;
    genParams["frequency_penalty"] = cfg.frequencyPenalty;
    genParams["num_predict"] = cfg.numPredict;
    genParams["num_ctx"] = cfg.numCtx;
    genParams["mirostat"] = cfg.mirostat;
    genParams["mirostat_tau"] = cfg.mirostatTau;
    genParams["mirostat_eta"] = cfg.mirostatEta;
    genParams["seed"] = cfg.seed;

    if (!cfg.stopWords.isEmpty()) {
        QJsonArray stopArray;
        for (const QString& word : cfg.stopWords)
            stopArray.append(word);
        genParams["stop"] = stopArray;
    }

    genParams["reasoning_effort"] = cfg.reasoningEffort;
    genParams["temperature_decay"] = cfg.temperatureDecay;
    genParams["repeat_last_n"] = cfg.repeatLastN;
    return genParams;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Initialize controllers (DI — inject service interfaces)
    m_statusController = new TaskStatusController(this);
    m_ttsController = new TtsController(this);
    m_sequenceRunner = new SequenceRunner(this, this, this, this, this, this, this, this, this);
    m_sequenceEditor = new SequenceEditorController(this, this, this, this);

    AppConfig startupCfg;
    startupCfg.load();
    const OllamaAdapter::Provider startupProvider =
        OllamaAdapter::providerFromString(startupCfg.provider);
    m_model.setProvider(startupProvider);
    m_agentModel.setProvider(startupProvider);

    QUrl startupEndpoint = startupCfg.endpoint;
    if (!startupEndpoint.isValid() || startupEndpoint.toString().trimmed().isEmpty())
        startupEndpoint = OllamaAdapter::defaultEndpointForProvider(startupProvider);
    m_model.setEndpoint(startupEndpoint);
    m_agentModel.setEndpoint(startupEndpoint);

    const QVariantMap startupParams = generationParamsFromConfig(startupCfg);
    const QString startupModel = startupCfg.selectedModel.trimmed();
    m_model.configure(startupModel, startupParams);
    m_agentModel.configure(startupModel, startupParams);

    // Build the UI. The strings in UiBuilder belong to the "UiBuilder" translation context,
    // which is where lupdate files them (with -tr-function-alias tr+=tr_fn).
    UIWidgets ui;
    UiBuilder::build(ui, [](const char* s) { return QCoreApplication::translate("UiBuilder", s); },
                     &m_model, &m_seqMgr, &m_templateMgr, m_ttsController, this);
    ide_widget = ui.ide_widget; m_rootEdit = ui.m_rootEdit;
    m_graphWindow = ui.m_graphWindow; m_seqGraphWindow = ui.m_seqGraphWindow;
    browseBtn = ui.browseBtn; m_files = ui.m_files;
    m_log = ui.m_log; m_chat = ui.m_chat; m_resultDisplayWidget = ui.m_resultDisplayWidget;
    m_contextStatusWidget = ui.m_contextStatusWidget; m_contextCommandsList = ui.m_contextCommandsList;
    m_contextGraphWidget = ui.m_contextGraphWidget; m_contextTabs = ui.m_contextTabs;
    m_settingsPanel = ui.m_settingsPanel;
    applyBtn = ui.applyBtn; undoBtn = ui.undoBtn; runSeqBtn = ui.runSeqBtn;
    // Always at hand: undoes the assistant's last change to the project files.
    undoBtn->setText(tr("Undo last change"));
    undoBtn->setToolTip(tr("Reverts the last change the assistant made to the project files "
                           "(up to 50 steps back)."));
    statusBar()->addPermanentWidget(undoBtn);
    undoBtn->show();
    rescanBtn = ui.rescanBtn; descBtn = ui.descBtn;
    settingsBtn = ui.settingsBtn; promptBtn = ui.promptBtn;
    editSeqBtn = ui.editSeqBtn; newSeqBtn = ui.newSeqBtn;
    runTemplateBtn = ui.runTemplateBtn; editTemplatesBtn = ui.editTemplatesBtn;
    exportSequenceAsTemplateBtn = ui.exportSequenceAsTemplateBtn;
    graphBtn = ui.graphBtn; seqGraphBtn = ui.seqGraphBtn;
    chatToggleBtn = ui.chatToggleBtn; logToggleBtn = ui.logToggleBtn;
    lightRagBtn = ui.lightRagBtn; confirmModeCombo = ui.confirmModeCombo;
    m_centerSplitter = ui.m_centerSplitter; mainSplitter = ui.mainSplitter;
    m_llmSettingsWindow = ui.m_llmSettingsWindow;

    setWindowTitle("DevAssistantQt");
    m_chat->setMainWindow(this);
    promptBtn->hide();
    QTabWidget *rootTabs = new QTabWidget(this);
    rootTabs->addTab(mainSplitter, tr("Assistant"));
    rootTabs->addTab(m_settingsPanel, tr("Settings"));
    setCentralWidget(rootTabs);

    // Signal connections (preserved from inline code)
    connect(ui.m_settingsSidebar, &QListWidget::currentRowChanged, ui.m_settingsPages, &QStackedWidget::setCurrentIndex);
    connect(graphBtn, &StyledButton::clicked, this, &MainWindow::openGraph);
    connect(seqGraphBtn, &StyledButton::clicked, this, [this]() { m_sequenceEditor->openSequenceGraph(); });
    connect(lightRagBtn, &StyledButton::clicked, this, &MainWindow::openLightRag);
    connect(ui.documentsBtn, &StyledButton::clicked, this, &MainWindow::openDocuments);
    connect(m_ttsController, &TtsController::statusMessage, this, [this](const QString& m){ log("TTS: "+m); });
    connect(m_ttsController, &TtsController::errorMessage, this, [this](const QString& e){ log("TTS Error: "+e); });
    connect(newSeqBtn, &StyledButton::clicked, this, [this]() { m_sequenceEditor->createNewSequence(); });
    connect(m_seqGraphWindow, &SequenceGraphWidget::sequenceSelected, this,
            [this](const QString& name) { m_sequenceEditor->openSequenceEditorForName(name); });
    connect(chatToggleBtn, &StyledButton::clicked, this, [this]() {
        bool v = !m_chat->isChatLogVisible(); m_chat->setChatLogVisible(v);
        chatToggleBtn->setText(v ? tr("Hide chat") : tr("Show chat"));
    });
    connect(logToggleBtn, &StyledButton::clicked, this, [this]() {
        bool v = !m_log->isVisible(); m_log->setVisible(v);
        logToggleBtn->setText(v ? tr("Hide log") : tr("Show log"));
    });
    connect(runTemplateBtn, &StyledButton::clicked, this, [this]() {
        refreshAllSequences(); ensureDefaultSequence();
        RunTemplateOrSequenceDialog dlg(m_seqMgr.sequences(), &m_templateMgr, this);
        dlg.setTemplateModeOnly();
        if (dlg.exec() != QDialog::Accepted) return;
        if (dlg.selectedMode() != RunTemplateOrSequenceDialog::RunTemplate) {
            QMessageBox::warning(this, tr("Error"), tr("Please choose a template.")); return;
        }
        QString sp = dlg.selectedDefaultSequencePath();
        if (sp.isEmpty() || !QFile::exists(sp)) { QMessageBox::warning(this, tr("Error"), tr("Sequence not found.")); return; }
        PromptSequence seq; QFile sf(sp);
        if (sf.open(QIODevice::ReadOnly)) seq = PromptSequence::fromJson(QJsonDocument::fromJson(sf.readAll()).object());
        if (seq.name().isEmpty()) { QMessageBox::warning(this, tr("Error"), tr("Could not load the sequence.")); return; }
        m_currentSeq = seq; m_currentStepIdx = 0; m_sequenceMode = true;
        m_sequenceEditor->updateSequenceGraphHighlight(); m_sequenceRunner->runNextStep();
    });
    connect(editTemplatesBtn, &StyledButton::clicked, this, [this]() {
        SolutionTemplateViewer *viewer = new SolutionTemplateViewer(&m_templateMgr, this);
        QDialog dialog(this); dialog.setWindowTitle(tr("Solution Templates")); dialog.resize(800, 600);
        QVBoxLayout *layout = new QVBoxLayout(&dialog); layout->addWidget(viewer);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(bb, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(bb); dialog.exec();
    });
    connect(exportSequenceAsTemplateBtn, &StyledButton::clicked, this, [this]() {
        refreshAllSequences();
        const QMap<QString, PromptSequence>& seqs = m_seqMgr.sequences();
        if (seqs.isEmpty()) { QMessageBox::information(this, tr("No sequences"), tr("No sequences found.")); return; }
        QStringList names; QMap<QString, QString> pb;
        for (auto &fp : m_seqMgr.fileList()) { names << QFileInfo(fp).fileName(); pb.insert(QFileInfo(fp).fileName(), fp); }
        bool ok = false;
        QString sel = QInputDialog::getItem(this, tr("Export"), tr("Choose:"), names, 0, false, &ok);
        if (!ok || sel.isEmpty()) return;
        PromptSequence seq = seqs.value(pb[sel]);
        QString tn = QInputDialog::getText(this, tr("Name"), tr("Template name:"), QLineEdit::Normal, seq.name(), &ok);
        if (!ok || tn.trimmed().isEmpty()) return;
        QString desc = QInputDialog::getMultiLineText(this, tr("Description"), tr("Description:"), seq.description(), &ok);
        if (!ok) desc = seq.description();
        QString td = QDir(m_templateMgr.root()).absoluteFilePath(tn.trimmed());
        if (QDir().exists(td)) { QMessageBox::warning(this, tr("Failed"), tr("A template with this name already exists.")); return; }
        if (m_templateMgr.exportSequenceAsTemplate(pb[sel], td, tn.trimmed(), desc)) {
            m_templateMgr.refresh(); QMessageBox::information(this, tr("OK"), tr("Exported."));
        } else { QMessageBox::warning(this, tr("Failed"), tr("Export failed.")); }
    });


    connect(browseBtn,   &StyledButton::clicked, this, &MainWindow::browse);
    connect(rescanBtn,   &StyledButton::clicked, this, &MainWindow::rescan);
    connect(settingsBtn, &StyledButton::clicked, this, &MainWindow::openSettings);
    connect(promptBtn,   &StyledButton::clicked, this, &MainWindow::editBasePrompt);
    connect(applyBtn,    &StyledButton::clicked, this, &MainWindow::applyChanges);
    connect(undoBtn,     &StyledButton::clicked, this, &MainWindow::undoLast);
    connect(m_llmSettingsWindow, &OllamaSettingsDialog::saveRequested, this, [this]() {
        if (!m_llmSettingsWindow)
            return;

        const OllamaAdapter::Provider provider =
            OllamaAdapter::providerFromString(m_llmSettingsWindow->provider());
        const bool providerChanged = (m_model.provider() != provider);

        m_model.setProvider(provider);
        m_agentModel.setProvider(provider);

        QUrl endpoint(m_llmSettingsWindow->endpoint().trimmed());
        if (!endpoint.isValid() || endpoint.toString().trimmed().isEmpty())
            endpoint = OllamaAdapter::defaultEndpointForProvider(provider);
        m_model.setEndpoint(endpoint);
        m_agentModel.setEndpoint(endpoint);

        const QVariantMap opts = m_llmSettingsWindow->params();
        const QString selectedModel = m_model.selectedModel().trimmed();
        const QString selectedAgentModel = m_agentModel.selectedModel().trimmed();
        if (!selectedModel.isEmpty())
            m_model.configure(selectedModel, opts);
        if (!selectedAgentModel.isEmpty())
            m_agentModel.configure(selectedAgentModel, opts);

        AppConfig cfg;
        cfg.load();
        cfg.provider = OllamaAdapter::providerToString(provider);
        cfg.endpoint = endpoint;
        cfg.params = opts;
        if (providerChanged)
            cfg.selectedModel.clear();

        cfg.temperature = opts.value("temperature", 1).toDouble();
        cfg.topP = opts.value("top_p", 0.9).toDouble();
        cfg.topK = opts.value("top_k", 40).toInt();
        cfg.repeatPenalty = opts.value("repeat_penalty", 1.1).toDouble();
        cfg.presencePenalty = opts.value("presence_penalty", 0.0).toDouble();
        cfg.frequencyPenalty = opts.value("frequency_penalty", 0.0).toDouble();
        cfg.numPredict = opts.value("num_predict", -1).toInt();
        cfg.numCtx = opts.value("num_ctx", 4096).toInt();
        cfg.mirostat = opts.value("mirostat", 0).toInt();
        cfg.mirostatTau = opts.value("mirostat_tau", 5.0).toDouble();
        cfg.mirostatEta = opts.value("mirostat_eta", 0.1).toDouble();
        cfg.seed = opts.value("seed", 0).toULongLong();

        QStringList stopWords;
        if (opts.contains("stop")) {
            const QJsonArray stopArray = opts.value("stop").toJsonArray();
            for (const QJsonValue& val : stopArray) {
                const QString word = val.toString().trimmed();
                if (!word.isEmpty())
                    stopWords << word;
            }
        }
        cfg.stopWords = stopWords;

        cfg.reasoningEffort = opts.value("reasoning_effort", "medium").toString();
        cfg.temperatureDecay = opts.value("temperature_decay", 0.0).toDouble();
        cfg.repeatLastN = opts.value("repeat_last_n", 64).toInt();
        cfg.save();

        if (m_chat)
            m_chat->refreshModelList();

        statusBar()->showMessage(tr("Provider: %1 | Model: %2")
                                     .arg(m_model.providerName(),
                                          m_model.selectedModel().trimmed().isEmpty()
                                              ? tr("<not selected>")
                                              : m_model.selectedModel().trimmed()));
        log(QString("LLM settings applied: provider=%1 endpoint=%2")
                .arg(m_model.providerName(), endpoint.toString()));
    });

    // Chat and model
    connect(m_chat, &ChatDialog::sendMessage, this, &MainWindow::sendUserMessage);
    connect(m_chat, &ChatDialog::sendMessageWithImages, this, &MainWindow::sendUserMessageWithImages);
    connect(m_chat, &ChatDialog::runTemplateRequested, this, &MainWindow::runTemplateRequested);
    connect(m_chat, &ChatDialog::runTemplateRequestedWithImages, this, &MainWindow::runTemplateRequestedWithImages);
    connect(m_chat, &ChatDialog::runSimpleChat, this, &MainWindow::runSimpleChat);
    connect(m_chat, &ChatDialog::runSimpleChatWithImages, this, &MainWindow::runSimpleChatWithImages);
    connect(m_chat, &ChatDialog::templateSelectionChanged, this, [this](const QString& path) { m_sequenceEditor->onChatTemplateSelectionChanged(path); });
    connect(&m_model, &OllamaAdapter::streamChunk, this, &MainWindow::onModelChunk);
    connect(&m_model, &OllamaAdapter::streamFinished, this, [this]() { m_sequenceRunner->handleAssistantFinished(); });
    connect(&m_model, &OllamaAdapter::streamError,
            this, [this](const QString &e){ log("Model error: " + e); });

    connect(runSeqBtn, &StyledButton::clicked, this, [this]() { m_sequenceRunner->runSequenceDialog(); });
    connect(editSeqBtn, &StyledButton::clicked, this, [this]() { m_sequenceEditor->editSequences(); });

    connect(descBtn, &StyledButton::clicked, this, &MainWindow::generateDescriptions);

    statusBar()->showMessage(tr("Provider: %1 | Model: %2")
                                 .arg(m_model.providerName(),
                                      m_model.selectedModel().trimmed().isEmpty()
                                          ? tr("<not selected>")
                                          : m_model.selectedModel().trimmed()));

    m_projectRoot = m_rootEdit->text();



    // Prompt sequences live in Documents/DevelopersAssistant/PromptSequences.
    QString seqRoot = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        .append("/DevelopersAssistant/PromptSequences");

    QDir seqDir(seqRoot);
    if (!seqDir.exists())
        seqDir.mkpath(".");

    m_seqMgr.setRoot(seqRoot);
    refreshAllSequences();
    ensureDefaultSequence();

    // Solution templates live in Documents/DevelopersAssistant/SolutionTemplates.
    QString tmplRoot = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        .append("/DevelopersAssistant/SolutionTemplates");

    QDir tmplDir(tmplRoot);
    if (!tmplDir.exists())
        tmplDir.mkpath(".");

    m_templateMgr.setRoot(tmplRoot);
    m_templateMgr.setSequenceManager(&m_seqMgr);  // Connect sequence manager to template manager
    m_templateMgr.refresh();

    ensureUniversalAgentTemplateInternal();
    m_templateMgr.refresh();

    // Update templates in ChatDialog after templates have been loaded
    m_chat->setTemplateManager(&m_templateMgr);

    // Initialize file system watcher to monitor external file changes
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onFileChanged);

    // Initialize the new ContextManager
    m_contextManager = new ContextManager(this);
    m_contextManager->setDevAssManager(&m_dev_manager);
    connect(m_contextManager, &ContextManager::contextChanged, this, [this]() {
        // When context changes, update the GUI display
        updateContextStatusDisplay();
    });
    updateContextStatusDisplay();

    connect(m_chat, &ChatDialog::signal_updatelastUserMessage, this, &MainWindow::slot_updatelastUserMessage);


    connect(ide_widget, &IDEWidget::signal_runSequenceDialog, this, [this]() { m_sequenceRunner->runSequenceDialog(); });

    connect(ide_widget, &IDEWidget::signal_updatelastUserMessage,this, &MainWindow::slot_updatelastUserMessage);

    connect(this, &MainWindow::missingFilesUpdated,
                ide_widget, &IDEWidget::updateMissingFiles);


    reOpenRootDir();

    if (m_centerSplitter) {
        m_centerSplitter->loadState();
        if (!m_centerSplitter->hasSavedState())
            m_centerSplitter->setSizes({430, 120, 230, 190});
    }
    mainSplitter->loadState();
    if (!mainSplitter->hasSavedState())
        mainSplitter->setSizes({280, 780, 380});

    m_sequenceEditor->openSequenceGraph();
    const QList<int> storedSizes = mainSplitter->sizes();
    if (storedSizes.size() >= 3 && storedSizes.at(2) == 0) {
        QList<int> fallbackSizes = storedSizes;
        fallbackSizes[0] = qMax(200, fallbackSizes.at(0));
        fallbackSizes[1] = qMax(400, fallbackSizes.at(1));
        fallbackSizes[2] = 250;
        mainSplitter->setSizes(fallbackSizes);
    }

    // Initialize task status controller (timer + status bar)
    m_statusController->initStatusBar(statusBar());
    m_statusController->setModel(&m_model);

    //rescan();
}


void MainWindow::openGraph()
{
    m_graphWindow->show();
    m_graphWindow->raise();
    m_graphWindow->activateWindow();
}

void MainWindow::openDocuments()
{
    // QPointer: the window deletes itself on close.
    if (!m_documentsWidget) {
        m_documentsWidget = new Rag::RagWidget();
        m_documentsWidget->setAttribute(Qt::WA_DeleteOnClose, true);
        m_documentsWidget->setFolder(projectRootPath());
    }
    m_documentsWidget->show();
    m_documentsWidget->raise();
    m_documentsWidget->activateWindow();
}

void MainWindow::openLightRag()
{
    // QPointer: the window deletes itself on close and the pointer then becomes null.
    if (!m_lightRagWidget) {
        m_lightRagWidget = new LightRagWidget();
        m_lightRagWidget->setAttribute(Qt::WA_DeleteOnClose, true);
    }

    m_lightRagWidget->show();
    m_lightRagWidget->raise();
    m_lightRagWidget->activateWindow();
}

void MainWindow::ensureUniversalAgentTemplateInternal()
{

        const QString templateDir = QDir(m_templateMgr.root()).filePath("UniversalAgent");
        const QString versionFilePath = QDir(templateDir).filePath(".universal_agent.version");
        const QString desiredVersion = QStringLiteral("3");

        if (!QDir().mkpath(templateDir)) {
            qCWarning(logUI) << "[MainWindow] Could not create template directory:" << templateDir;
            return;
        }

        bool needsRewrite = true;
        {
            QFile versionFile(versionFilePath);
            if (versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString currentVersion = QString::fromUtf8(versionFile.readAll()).trimmed();
                versionFile.close();
                if (currentVersion == desiredVersion)
                    needsRewrite = false;
            }
        }
        if (!needsRewrite)
            return;

        const auto writeSequence = [&](const QString& fileName, const PromptSequence& seq) -> bool {
            const QString path = QDir(templateDir).filePath(fileName);
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qCWarning(logUI) << "[MainWindow] Could not write universal template sequence file:" << path;
                return false;
            }
            file.write(QJsonDocument(seq.toJson()).toJson(QJsonDocument::Indented));
            file.close();
            return true;
        };

        const QStringList allCommands = {
            "ASK_USER", "OPEN_FILE", "OPEN_FILE_REGION",
            "SEARCH_PROJECT", "SEARCH_PROJECT_CASE_INSENSITIVE", "SEARCH_WEB",
            "LIST_FILES_IN_FOLDER", "CONTEXT_SHOW", "CONTEXT_CLEAR",
            "CREATE_FILE", "REPLACE_IN_FILE", "EDIT_IN_RANGE", "INSERT_AT_LINE", "APPEND_TO_FILE",
            "RENAME_FILE", "COPY_FILE", "DELETE_FILE",
            "CREATE_DIR", "DELETE_DIR", "APPLY_PATCH",
            "RUN_SHELL", "RUN_PYTHON",
            "GIT_STATUS", "GIT_DIFF", "GIT_RESTORE_FILE", "UNDO_LAST_CHANGE",
            "WRITE_DESCRIPTION", "OPEN_SEQUENCE", "CALL_SEQUENCE", "REASONING",
            "SHOW_RESULT", "OPEN_APP", "OPEN_URL", "CLIPBOARD_SET", "CLIPBOARD_GET"
        };

        PromptSequence mainSeq(QStringLiteral("Universal Agent Main"));
        mainSeq.setDescription(QStringLiteral("Universal multi-stage flow: analysis -> context -> implementation -> verification."));
        {
            PromptStep s1;
            s1.setTemplate(
                "Current date: {{CURRENT_DATE}}\n"
                "Current datetime: {{CURRENT_DATETIME}}\n\n"
                "User request:\n{{USER_QUERY}}\n\n"
                "Context:\n{{HISTORY}}\n{{CONTEXT_CHAIN}}\n\n"
                "First, produce a short plan and identify missing context.\n"
                "If context is missing, call SEARCH_PROJECT/OPEN_FILE_REGION/SEARCH_WEB.\n"
                "If this is mostly informational/search task, produce SHOW_RESULT after collecting enough evidence.\n"
                "If full implementation is needed, use OPEN_SEQUENCE with sequenceName from {{OPEN_SEQUENCE_INFO}}.\n"
                "Do not repeat the same SEARCH_WEB query in a loop.\n"
                "Return JSON commands only.\n\n"
                "Allowed commands:\n{{SUPPORTED_COMMANDS}}"
            );
            s1.setAllowedCommands(allCommands);
            mainSeq.addStep(s1);
        }
        {
            PromptStep s2;
            s2.setTemplate(
                "Build an execution plan based on gathered context.\n"
                "Project structure:\n{{PROJECT_STRUCTURE}}\n\n"
                "Open files:\n{{OPEN_FILES}}\n\n"
                "If the user request is already answerable, emit SHOW_RESULT now.\n"
                "Otherwise decide the next focused sequence using OPEN_SEQUENCE when useful (analysis/coding/testing).\n"
                "Never end this step with only repeated search commands.\n"
                "Return JSON commands only."
            );
            s2.setAllowedCommands(allCommands);
            mainSeq.addStep(s2);
        }

        PromptSequence fileAnalysisSeq(QStringLiteral("Universal Agent File Analysis"));
        fileAnalysisSeq.setDescription(QStringLiteral("Deep file-level and architecture analysis for the task."));
        {
            PromptStep s;
            s.setTemplate(
                "Analyze project files deeply for this request:\n{{USER_QUERY}}\n\n"
                "Use FILE_LIST, PROJECT_STRUCTURE, OPEN_FILE_REGION and SEARCH_PROJECT to find exact implementation points.\n"
                "Summarize hotspots and then OPEN_SEQUENCE to context-chain or code-generation.\n"
                "If this request is informational and no code edits are needed, emit SHOW_RESULT.\n"
                "Return JSON commands only.\n\n"
                "{{FILE_LIST}}\n{{PROJECT_STRUCTURE}}\n{{OPEN_SEQUENCE_INFO}}"
            );
            s.setAllowedCommands(allCommands);
            fileAnalysisSeq.addStep(s);
        }

        PromptSequence contextChainSeq(QStringLiteral("Universal Agent Context Chain"));
        contextChainSeq.setDescription(QStringLiteral("Use executed command context to guide the next actions."));
        {
            PromptStep s;
            s.setTemplate(
                "Analyze context chain and previous results:\n{{CONTEXT_CHAIN}}\n\n"
                "Detect outdated assumptions, missing checks, and next best action.\n"
                "If enough evidence is collected and no more actions are needed, emit SHOW_RESULT.\n"
                "If code changes are ready, OPEN_SEQUENCE to code-generation.\n"
                "If verification is needed, OPEN_SEQUENCE to testing.\n"
                "Return JSON commands only."
            );
            s.setAllowedCommands(allCommands);
            contextChainSeq.addStep(s);
        }

        PromptSequence codeGenSeq(QStringLiteral("Universal Agent Code Generation"));
        codeGenSeq.setDescription(QStringLiteral("Generate and refine code changes with minimal risk."));
        {
            PromptStep s1;
            s1.setTemplate(
                "Implement the requested change.\n"
                "Request: {{USER_QUERY}}\n"
                "Context files:\n{{OPEN_FILES}}\n\n"
                "Prefer minimal diffs, preserve architecture, and avoid unrelated edits.\n"
                "Create/edit files and run local checks as needed.\n"
                "Return JSON commands only."
            );
            s1.setAllowedCommands(allCommands);
            codeGenSeq.addStep(s1);
        }
        {
            PromptStep s2;
            s2.setTemplate(
                "Review your produced changes for regressions and obvious defects.\n"
                "Use GIT_DIFF/GIT_STATUS and targeted checks.\n"
                "Then OPEN_SEQUENCE to testing if validation is needed.\n"
                "If validation is already sufficient, emit SHOW_RESULT.\n"
                "Return JSON commands only."
            );
            s2.setAllowedCommands(allCommands);
            codeGenSeq.addStep(s2);
        }

        PromptSequence testingSeq(QStringLiteral("Universal Agent Testing"));
        testingSeq.setDescription(QStringLiteral("Validate implementation and provide final status."));
        {
            PromptStep s1;
            s1.setTemplate(
                "Run verification for the implemented change.\n"
                "Use available test/build/lint commands and summarize failures.\n"
                "If failures require fixes, OPEN_SEQUENCE to code-generation.\n"
                "If no runnable tests exist, still produce a concise SHOW_RESULT with manual verification notes.\n"
                "Return JSON commands only."
            );
            s1.setAllowedCommands(allCommands);
            testingSeq.addStep(s1);
        }
        {
            PromptStep s2;
            s2.setTemplate(
                "Prepare final concise result for the user.\n"
                "Include what changed, how validated, and residual risks.\n"
                "Use SHOW_RESULT for final message (mandatory).\n"
                "Return JSON commands only."
            );
            s2.setAllowedCommands(allCommands);
            testingSeq.addStep(s2);
        }

        if (!writeSequence(QStringLiteral("main.seq.json"), mainSeq) ||
            !writeSequence(QStringLiteral("file-analysis.seq.json"), fileAnalysisSeq) ||
            !writeSequence(QStringLiteral("context-chain.seq.json"), contextChainSeq) ||
            !writeSequence(QStringLiteral("code-generation.seq.json"), codeGenSeq) ||
            !writeSequence(QStringLiteral("testing.seq.json"), testingSeq)) {
            return;
        }

        SolutionTemplate tmpl(QStringLiteral("Universal Agent"));
        tmpl.setDescription(QStringLiteral("Universal multi-sequence agent for coding, search, editing and verification."));
        tmpl.setMainSequencePath(QStringLiteral("main.seq.json"));
        tmpl.addSequence(QStringLiteral("main.seq.json"));
        tmpl.addSequence(QStringLiteral("file-analysis.seq.json"));
        tmpl.addSequence(QStringLiteral("context-chain.seq.json"));
        tmpl.addSequence(QStringLiteral("code-generation.seq.json"));
        tmpl.addSequence(QStringLiteral("testing.seq.json"));

        tmpl.setTargetsForSequence(QStringLiteral("main"),
                                   QJsonArray{QStringLiteral("file-analysis"), QStringLiteral("context-chain"),
                                              QStringLiteral("code-generation"), QStringLiteral("testing")});
        tmpl.setTargetsForSequence(QStringLiteral("file-analysis"),
                                   QJsonArray{QStringLiteral("context-chain"), QStringLiteral("code-generation")});
        tmpl.setTargetsForSequence(QStringLiteral("context-chain"),
                                   QJsonArray{QStringLiteral("code-generation"), QStringLiteral("testing"), QStringLiteral("main")});
        tmpl.setTargetsForSequence(QStringLiteral("code-generation"),
                                   QJsonArray{QStringLiteral("testing"), QStringLiteral("main")});
        tmpl.setTargetsForSequence(QStringLiteral("testing"),
                                   QJsonArray{QStringLiteral("code-generation"), QStringLiteral("main")});

        if (!m_templateMgr.save(tmpl, templateDir)) {
            qCWarning(logUI) << "[MainWindow] Could not save universal template metadata:" << templateDir;
            return;
        }

        QFile versionFile(versionFilePath);
        if (!versionFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCWarning(logUI) << "[MainWindow] Could not write universal template version file:" << versionFilePath;
        } else {
            versionFile.write(desiredVersion.toUtf8());
            versionFile.close();
        }

        qCDebug(logUI) << "[MainWindow] UniversalAgent template upgraded to version" << desiredVersion;
}


void MainWindow::browse(){
    QString d = QFileDialog::getExistingDirectory(this, tr("Select project folder"), m_projectRoot);
    if (d.isEmpty()) return;

    m_projectRoot = d;
    reOpenRootDir();
}

void MainWindow::rescan(){
    // Load .dapro before scanning.
    m_dev_manager.load();

    // Files inside the DevelopersAssistant folder are skipped.
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    const QString projectRootPath = m_projectRoot;

    m_index.setRoot(m_projectRoot);
    m_files->clear();

    QSet<QString> needRed_set;
    for (const QString& rel : m_index.files()) {
        const QString absPath = QDir(projectRootPath).filePath(rel);

        if (absPath.startsWith(devAssPath))
            continue;

        QListWidgetItem* item = new QListWidgetItem(QDir::fromNativeSeparators(rel), m_files);

        FileDescription* fd = m_dev_manager.findFileDescription(absPath);
        QDateTime fileMod = QFileInfo(absPath).lastModified();

        // Red: the file has no description or it is out of date. Green: up to date.
        bool needRed = false;
        if (!fd) {
            needRed = true;
        } else if (fileMod > fd->lastModTime) {
            needRed = true;
        }

        if (needRed){
            needRed_set.insert(absPath);
            item->setBackground(QColor(0xFD, 0xE8, 0xE8));   // no description yet, or out of date
        }
        else
            item->setBackground(QColor(0xE3, 0xF6, 0xEA));   // description up to date

        item->setData(Qt::UserRole, absPath);
    }

    emit missingFilesUpdated(needRed_set);



    log("Rescanned project.");
}

void MainWindow::openSettings(){
    if (!m_llmSettingsWindow)
        return;

    if (m_llmSettingsWindow->isVisible()) {
        m_llmSettingsWindow->hide();
        settingsBtn->setText(tr("Open LLM settings"));
        return;
    }

    m_llmSettingsWindow->reloadFromConfig();
    m_llmSettingsWindow->show();
    settingsBtn->setText(tr("Hide LLM settings"));
}

void MainWindow::editBasePrompt(){
    PromptEditorDialog d(&m_promptMgr, this);
    d.exec();
}

QString MainWindow::buildProjectContext() const
{
    const QString projectRootPath = m_projectRoot;
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    QString s = "Project root: " + projectRootPath + "\nFiles:\n";

    int file_counter = 0;
    for (const QString& f : m_index.files()) {
        const QString abs = QDir(projectRootPath).filePath(f);

        // Skip the DevelopersAssistant folder and .dadescr files.
        if (abs.startsWith(devAssPath) ||
            f.startsWith(QStringLiteral("DevelopersAssistant") + QDir::separator()) ||
            f.endsWith(QStringLiteral(".dadescr")))
            continue;

        s += " - " + QDir::toNativeSeparators(abs) + '\n';
        ++file_counter;

        // The file's description, if one has been generated.
        const QString rel = QDir(projectRootPath).relativeFilePath(abs);
        const QString descrPath =
            m_dev_manager.devAssDir().filePath(rel + ".dadescr");
        QFile descrFile(descrPath);
        if (descrFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString descr = QString::fromUtf8(descrFile.readAll());
            descrFile.close();

            if (!descr.isEmpty()) {
                s += "   Description:\n";
                s += "   " + descr.replace("\n", "\n   ") + "\n";
            }
        }

        // File contents are not included here.
        /*QFile fContent(abs);
        if (fContent.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(fContent.readAll());
            fContent.close();

            if (!content.isEmpty()) {
                s += "   Contents:\n";
                s += "   " + content.replace("\n", "\n   ") + "\n";
            }
        }*/
    }

    if(file_counter == 0){
        s += QStringLiteral("The project has no files yet.\n");
    }

    // Files opened with OPEN_FILE / OPEN_FILE_REGION.
    if (!m_contextCache.contextContent().isEmpty())
        s += "\nContents of files added to the context:\n" + m_contextCache.contextContent() + "\n";

    // Context chain from executed commands.
    if (m_contextManager) {
        QString contextChain = m_contextManager->generateContextText();
        if (!contextChain.isEmpty()) {
            s += "\nContext chain from executed commands:\n" + contextChain + "\n";
        }
    }

    return s;
}

void MainWindow::applyChanges(){
    if (m_pending.isEmpty()) { QMessageBox::information(this, tr("Nothing to apply"), tr("No pending changes.")); return; }
    ChangesDialog dlg(this);
    dlg.setChanges(m_pending);
    if (dlg.exec()==QDialog::Accepted) {
        QString err;
        QList<Change> copy = m_pending;
        if (!m_applier.apply(copy, &err)) { QMessageBox::critical(this, tr("Apply failed"), err); return; }
        log(QString("Applied %1 change(s). ").arg(m_pending.size()));
        m_pending.clear();
        rescan();
    } else {
        log("Changes canceled.");
    }
}

void MainWindow::undoLast(){
    // The commands apply their changes through DevAssManager, so that is where the history is.
    QString err;
    if (!m_dev_manager.undoLastChange(&err)) {
        QMessageBox::information(this, tr("Undo"), err.isEmpty() || err == QLatin1String("Nothing to undo")
                                 ? tr("There is nothing to undo.") : err);
        return;
    }
    log("Undo done.");
    statusBar()->showMessage(tr("The last change was undone."), 5000);
    rescan();
}

void MainWindow::sendUserMessage(const QString& text){
    if (m_contextManager)
        m_contextManager->observeUserQuery(text);

    m_lastUserMessage = text;
    resetSequenceStabilityState();
    qCDebug(logUI) << "[sendUserMessage] updated last user message =" << m_lastUserMessage;
    appendUserQuestionToResultDisplay(text);

    // Add to main history
    m_history.push_back(qMakePair(QString("user"), text));
    // Also add to separate user messages storage (for LAST_USER_MESSAGES resolver)
    m_lastUserMessages.append(text);
    // Keep only last N messages
    while (m_lastUserMessages.size() > MAX_USER_MESSAGES) {
        m_lastUserMessages.removeFirst();
    }
    // Debug info about LAST_USER_MESSAGES storage
    qCDebug(logUI) << "[sendUserMessage] Added message to LAST_USER_MESSAGES storage. Current count:" << m_lastUserMessages.size();
    qCDebug(logUI) << "[sendUserMessage] Last message in storage:" << (m_lastUserMessages.isEmpty() ? QString("empty") : m_lastUserMessages.last());

    // Debug info about managers before running sequence
    qCDebug(logUI) << "[sendUserMessage] About to call runSequenceDialog";
    qCDebug(logUI) << "[sendUserMessage] Template manager root:" << m_templateMgr.root();
    qCDebug(logUI) << "[sendUserMessage] Sequence manager root:" << m_seqMgr.root();

    runSequenceDialog();

    qCDebug(logUI) << "[sendUserMessage] runSequenceDialog completed";

}

void MainWindow::onModelChunk(const QString& s){
    m_streamBuf += s;
    if (m_chat) m_chat->appendAssistantChunk(s);
}

QByteArray MainWindow::extractJsonPart(const QByteArray& txt) const
{
    QList<QByteArray> parts = splitConcatenatedJson_fromText(QString::fromUtf8(txt));
    if (parts.isEmpty())
        return QByteArray();

    if (parts.size() == 1) {
        const QByteArray trimmed = parts.first().trimmed();
        if (!trimmed.isEmpty() && trimmed.startsWith('['))
            return trimmed;
    }

    QByteArray joined = "[";
    for (int i = 0; i < parts.size(); ++i) {
        if (i)
            joined += ',';
        joined += parts.at(i);
    }
    joined += "]";
    return joined;
}

void MainWindow::handleOpenCommand(Command* cmd)
{
    const QJsonObject params = cmd->params();
    const QString rawPath = params.value("path").toString();
    if (rawPath.isEmpty())
        return;

    const QString normalized = normalizeContextPath(rawPath, m_projectRoot);
    QFile file(normalized);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString text = QString::fromUtf8(file.readAll());
    file.close();

    QStringList allLines = text.split('\n', Qt::KeepEmptyParts);
    if (text.endsWith('\n') && !allLines.isEmpty())
        allLines.removeLast();
    if (cmd->name() == "OPEN_FILE") {
        m_contextCache.setFullFileContext(normalized, allLines);
        // Track that this file's content is in context from OPEN_FILE command
        m_fileToContextCommands[normalized].insert("OPEN_FILE");
        // Add file to watcher to monitor for external changes
        if (!m_fileWatcher->files().contains(normalized) && QFileInfo::exists(normalized)) {
            m_fileWatcher->addPath(normalized);
        }
        // Add command to context manager
        if (m_contextManager) {
            QString result = QString("OPEN_FILE: %1 (full content)").arg(rawPath);
            addCommandToContext(cmd, result);
        }
        return;
    }

    int startLine = params.value("startLine").toInt(-1);
    int endLine = params.value("endLine").toInt(-1);
    QStringList snippet;

    if (startLine >= 1 && endLine >= startLine) {
        // Validate that the requested line numbers don't exceed the total number of lines in the file
        if (startLine > allLines.size()) {
            // Error: requested start line is beyond the end of the file
            QString errorMsg = QString("ERROR: Cannot OPEN_FILE_REGION %1: requested start line %2 is beyond file length of %3 lines. Remember: line numbering starts from 1, not 0.")
                    .arg(rawPath).arg(startLine).arg(allLines.size());
            log(errorMsg);
            if (m_contextManager) {
                addCommandToContext(cmd, errorMsg);
            }
            return;
        }

        if (endLine > allLines.size()) {
            // Error: requested end line is beyond the end of the file
            QString errorMsg = QString("ERROR: Cannot OPEN_FILE_REGION %1: requested end line %2 is beyond file length of %3 lines. Remember: line numbering starts from 1, not 0.")
                    .arg(rawPath).arg(endLine).arg(allLines.size());
            log(errorMsg);
            if (m_contextManager) {
                addCommandToContext(cmd, errorMsg);
            }
            return;
        }

        startLine = qMax(1, startLine);
        endLine = qMin(endLine, allLines.size());
        snippet = allLines.mid(startLine - 1, endLine - startLine + 1);
    } else {
        const QString region = params.value("region").toString();
        if (region.isEmpty())
            return;
        QRegularExpression re(region, QRegularExpression::DotMatchesEverythingOption
                                        | QRegularExpression::MultilineOption);
        auto match = re.match(text);
        if (!match.hasMatch())
            return;
        const int startIndex = match.capturedStart();
        startLine = text.left(startIndex).count('\n') + 1;
        const QString captured = match.captured(0);
        snippet = captured.split('\n', Qt::KeepEmptyParts);
        if (captured.endsWith('\n') && !snippet.isEmpty())
            snippet.removeLast();
    }

    if (!snippet.isEmpty()) {
        m_contextCache.addRegionContext(normalized, startLine, snippet);
        // Track that this file's content is in context from OPEN_FILE_REGION command
        m_fileToContextCommands[normalized].insert("OPEN_FILE_REGION");
        // Add file to watcher to monitor for external changes
        if (!m_fileWatcher->files().contains(normalized) && QFileInfo::exists(normalized)) {
            m_fileWatcher->addPath(normalized);
        }
        // Add command to context manager - include the actual numbered content
        if (m_contextManager) {
            QString numberedContent = m_contextCache.formatNumberedLines(snippet, startLine);
            QString result = QString("OPEN_FILE_REGION: %1\n%2").arg(rawPath, numberedContent);
            addCommandToContext(cmd, result);
        }
    }
}


bool MainWindow::applyGroupedEditInRange(const QList<Command*>& commands, QString* err)
{
    QMap<QString, QList<EditInRangeCommand*>> grouped;
    for (Command* c : commands) {
        auto edit = dynamic_cast<EditInRangeCommand*>(c);
        if (!edit)
            continue;
        const QString normalized = normalizeContextPath(edit->path(), m_projectRoot);
        grouped[normalized].append(edit);
    }

    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        if (!EditInRangeCommand::applyBatch(it.value(), &m_dev_manager, err))
            return false;
        m_contextCache.clearContextForPath(it.key());
    }
    return true;
}

void MainWindow::handlePostApplyContextCleanup(Command* cmd)
{
    const QString name = cmd->name();
    const QJsonObject p = cmd->params();
    auto clear = [&](const QString& key) {
        if (!key.isEmpty())
            m_contextCache.clearContextForPath(key);
    };

    if (name == "EDIT_IN_RANGE" || name == "REPLACE_IN_FILE"
        || name == "DELETE_FILE" || name == "CREATE_FILE"
        || name == "APPEND_TO_FILE" || name == "INSERT_AT_LINE") {
        clear(p.value("path").toString());
        return;
    }

    if (name == "RENAME_FILE") {
        clear(p.value("oldPath").toString());
        clear(p.value("newPath").toString());
        return;
    }

    if (name == "COPY_FILE") {
        clear(p.value("to").toString());
        return;
    }

    if (name == "GIT_RESTORE_FILE") {
        clear(p.value("path").toString());
        return;
    }

    if (name == "APPLY_PATCH") {
        m_contextCache.clearAll();
        return;
    }

    if (name == "UNDO_LAST_CHANGE") {
        // We don't know which files changed; clear all file-context slices.
        m_contextCache.clearAll();
        return;
    }

    // Handle RunShellCommand - remove old output when command is run again
    if (name == "RUN_SHELL") {
        QString command = p.value("command").toString();
        if (!command.isEmpty()) {
            m_contextCache.removeConsoleLogsWithPrefix("==== RUN_SHELL: " + command);
            m_shellCommandOutputs.remove(command);
        }
        return;
    }

    // Handle RunPythonCommand - remove old output when same script is run again
    if (name == "RUN_PYTHON") {
        QString scriptPath = p.value("scriptPath").toString();
        QString code = p.value("code").toString();
        QString identifier = !scriptPath.isEmpty() ? scriptPath : "inline code";

        if (!identifier.isEmpty()) {
            m_contextCache.removeConsoleLogsWithPrefix("==== RUN_PYTHON: " + identifier);
            m_pythonScriptOutputs.remove(identifier);
        }
        return;
    }

    // Handle OpenFileContextCommand and OpenFileRegionCommand - add to file watcher
    if (name == "OPEN_FILE" || name == "OPEN_FILE_REGION") {
        QString path = p.value("path").toString();
        if (!path.isEmpty()) {
            const QString normalized = normalizeContextPath(path, m_projectRoot);
            // Add file to watcher to monitor for external changes
            if (!m_fileWatcher->files().contains(normalized) && QFileInfo::exists(normalized)) {
                m_fileWatcher->addPath(normalized);
            }
        }
    }
}

void MainWindow::handleSearchResults(Command* cmd)
{
    if (auto web = dynamic_cast<SearchWebCommand*>(cmd)) {
        if (web->results().isEmpty())
            return;
        QString query = cmd->params().value("query").toString();
        QString title = QString("==== SEARCH_WEB: %1 ====").arg(query);
        QString body = web->results().join("\n\n");
        qCDebug(logUI) << "[MainWindow] SEARCH_WEB results count:" << web->results().size();
        qCDebug(logUI) << "[MainWindow] SEARCH_WEB result body length:" << body.length();
        if (!body.isEmpty()) {
            qCDebug(logUI) << "[MainWindow] SEARCH_WEB first 200 chars of result:" << body.left(200);
        } else {
            qCDebug(logUI) << "[MainWindow] SEARCH_WEB result body is empty";
        }

        // Matches the phrase SearchWebCommand writes when it found results but no page content
        // ("... there is no content to summarize."). Keep the two in sync.
        bool needsSummarization = body.contains("content to summarize", Qt::CaseInsensitive);

        QString resultBody = body;
        if (needsSummarization) {
            resultBody = summarizeSearchResults(body, query);
        }

        appendExternalContext(QString("%1\n%2").arg(title, resultBody));

        // Add to context manager
        if (m_contextManager) {
            qCDebug(logUI) << "[MainWindow] Adding SEARCH_WEB command to context manager";
            addCommandToContext(cmd, QString("%1\n%2").arg(title, resultBody));
        } else {
            qCDebug(logUI) << "[MainWindow] Context manager is null, skipping context addition";
        }
        return;
    }
    if (auto local = dynamic_cast<SearchProjectCommand*>(cmd)) {
        if (local->matches().isEmpty())
            return;
        QString title = QString("==== SEARCH_PROJECT: %1 ====").arg(cmd->params().value("query").toString());
        QString body = local->matches().join("\n\n");
        appendExternalContext(QString("%1\n%2").arg(title, body));

        // Add to context manager
        if (m_contextManager) {
            addCommandToContext(cmd, QString("%1\n%2").arg(title, body));
        }
    }
}

QString MainWindow::searchResultsText() const
{
    return m_contextCache.externalSources().join("\n\n");
}

void MainWindow::appendConsoleContext(const QString& title,
                                      const QString& stdOut,
                                      const QString& stdErr,
                                      int exitCode)
{
    QString block = QString("==== %1 ====\n").arg(title.isEmpty() ? "Console" : title);
    if (!stdOut.trimmed().isEmpty()) {
        block += "[stdout]\n" + stdOut.trimmed() + "\n";
    }
    if (!stdErr.trimmed().isEmpty()) {
        block += "[stderr]\n" + stdErr.trimmed() + "\n";
    }
    block += QString("exitCode=%1").arg(exitCode);

    // If this is a RUN_SHELL command output, replace existing output for the same command
    if (title.startsWith("RUN_SHELL: ")) {
        QString cmd = title.mid(11); // Extract the command part after "RUN_SHELL: "
        m_contextCache.removeConsoleLogsWithPrefix("==== RUN_SHELL: " + cmd);
        m_shellCommandOutputs.remove(cmd);
    }
    // If this is a RUN_PYTHON command output, replace existing output for the same script
    else if (title.startsWith("RUN_PYTHON: ")) {
        QString script = title.mid(12); // Extract the script part after "RUN_PYTHON: "
        m_contextCache.removeConsoleLogsWithPrefix("==== RUN_PYTHON: " + script);
        m_pythonScriptOutputs.remove(script);
    }

    m_contextCache.appendConsoleLog(block.trimmed());
    // Keep the 5 most recent logs: the newest output is what the model needs next.
    const auto logs = m_contextCache.consoleLogs();
    if (logs.size() > 5) {
        m_contextCache.clearConsoleLogs();
        for (int i = logs.size() - 5; i < logs.size(); ++i)
            m_contextCache.appendConsoleLog(logs.at(i));
    }
}

void MainWindow::appendExternalContext(const QString& block)
{
    if (block.trimmed().isEmpty())
        return;
    m_contextCache.appendExternalSource(block.trimmed());
    // Keep the 5 most recent blocks.
    const auto sources = m_contextCache.externalSources();
    if (sources.size() > 5) {
        m_contextCache.clearExternalSources();
        for (int i = sources.size() - 5; i < sources.size(); ++i)
            m_contextCache.appendExternalSource(sources.at(i));
    }
}

QString MainWindow::commandBatchSignature(const QList<Command*>& commands) const
{
    return m_sequenceRunner->commandBatchSignature(commands);
}

bool MainWindow::isReadOnlyCommandBatch(const QList<Command*>& commands) const
{
    return m_sequenceRunner->isReadOnlyCommandBatch(commands);
}

bool MainWindow::isImplementationIntentText(const QString& text)
{
    return SequenceRunner::isImplementationIntentText(text);
}

bool MainWindow::maybeAutoRepairAfterExecution(const PromptStep& step,
                                              const QDateTime& execStartedAt,
                                              const QList<Command*>& executedCommands,
                                              bool applyFailed,
                                              const QString& lastError)
{
    return m_sequenceRunner->maybeAutoRepairAfterExecution(step, execStartedAt, executedCommands, applyFailed, lastError);
}

void MainWindow::generateDescriptions()
{
    DescriptionGenerator generator(&m_model, &m_dev_manager, this);

    // Collect the project files whose description is missing or out of date.
    QStringList fileList;
    QDirIterator it(m_projectRoot, QDir::Files, QDirIterator::Subdirectories);
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    while (it.hasNext()) {
        const QString srcFilePath = it.next();
        if (srcFilePath.startsWith(devAssPath))
            continue;

        FileDescription* fd = m_dev_manager.findFileDescription(srcFilePath);
        QDateTime fileMod = QFileInfo(srcFilePath).lastModified();

        if (fd && fd->isDescriptionUpToDate && fileMod <= fd->lastModTime)
            continue;

        fileList << srcFilePath;
    }

    if (fileList.isEmpty()) {
        QMessageBox::information(this, tr("Done"),
                                 tr("No files in the project need a description."));
        return;
    }

    QProgressDialog prog(tr("Generating descriptions…"), tr("Cancel"),
                         0, fileList.size(), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);

    int processed = 0, failed = 0;
    for (const QString &srcFilePath : fileList) {
        if (prog.wasCanceled()) break;

        QString descr = generator.generateAndSave(srcFilePath);
        if (descr.isEmpty()){
            ++failed;
        }
        else{
            QString relPath = QDir(m_projectRoot).relativeFilePath(srcFilePath);
            QString descrPath = m_dev_manager.devAssDir().filePath(relPath + ".dadescr");
            QString descrFileName = QFileInfo(descrPath).fileName();
            log(QString("✓ Described %1 → %2").arg(relPath).arg(descrFileName));
        }
        ++processed;
        prog.setValue(processed);
    }

    prog.setValue(fileList.size());
    log(QString("Description generation finished: %1 succeeded, %2 failed.")
        .arg(processed - failed).arg(failed));

    rescan();
}

void MainWindow::log(const QString& m){ m_log->append(m); }

// Text typed in the chat or recognized from speech.
void MainWindow::slot_updatelastUserMessage(const QString &lastUserMessage)
{
    if (lastUserMessage.trimmed().isEmpty())
        return;
    m_lastUserMessage = lastUserMessage;
    qCDebug(logUI)<<"m_lastUserMessage = "<<m_lastUserMessage;
}

void MainWindow::openFileInEditor(const QString& path)
{
    ide_widget->openFile(QDir(m_projectRoot).absoluteFilePath(path));
}

void MainWindow::openProject(const QString& path)
{
    m_projectRoot = QDir::cleanPath(QDir(path).absolutePath());
    reOpenRootDir();
}

/* ────────────────────────────────────────────────────── */
void MainWindow::reOpenRootDir(){
    if(m_projectRoot.isEmpty()){
        return;
    }

    // Clear file watcher and context mappings when switching projects
    m_fileWatcher->removePaths(m_fileWatcher->files());
    m_fileToContextCommands.clear();
    m_shellCommandOutputs.clear();
    m_pythonScriptOutputs.clear();

    ide_widget->openProjectbyPath(m_projectRoot);

    m_rootEdit->setText(m_projectRoot);
    m_dev_manager.setProjectRoot(m_projectRoot);
    m_promptMgr.setProjectRoot(m_projectRoot);

    if (!m_dev_manager.createFolder() || !m_dev_manager.createDaproFile())
    {
        qCritical(logUI) << "Could not prepare the DevelopersAssistant folder / .dapro file";
        //return -1;
    }

    if (!m_dev_manager.generateDescrFiles())
    {
        qCritical(logUI) << "Could not create the .dadescr files";
    }

    rescan();
}

void MainWindow::setupModelWithGenerationParams()
{
    AppConfig cfg;
    cfg.load();

    QVariantMap genParams;
    genParams["temperature"] = cfg.temperature;
    genParams["top_p"] = cfg.topP;
    genParams["top_k"] = cfg.topK;
    genParams["repeat_penalty"] = cfg.repeatPenalty;
    genParams["presence_penalty"] = cfg.presencePenalty;
    genParams["frequency_penalty"] = cfg.frequencyPenalty;
    genParams["num_predict"] = cfg.numPredict;
    genParams["num_ctx"] = cfg.numCtx;
    genParams["mirostat"] = cfg.mirostat;
    genParams["mirostat_tau"] = cfg.mirostatTau;
    genParams["mirostat_eta"] = cfg.mirostatEta;
    genParams["seed"] = cfg.seed;

    if (!cfg.stopWords.isEmpty()) {
        QJsonArray stopArray;
        for (const QString& word : cfg.stopWords) {
            stopArray.append(word);
        }
        genParams["stop"] = stopArray;
    }

    genParams["reasoning_effort"] = cfg.reasoningEffort;
    genParams["temperature_decay"] = cfg.temperatureDecay;
    genParams["repeat_last_n"] = cfg.repeatLastN;

    if (!m_model.selectedModel().isEmpty()) {
        m_model.configure(m_model.selectedModel(), genParams);
    }
}



QString MainWindow::previousStepsText() const
{
    if (m_prevSteps.isEmpty()) return QString("Nothing has been done yet.");
    QString out;
    for (const QString& s : m_prevSteps) out += " - " + s + "\n";
    return out.trimmed();
}

QString MainWindow::fileDescriptionsText() const
{
    QString out;
    QDir dev = m_dev_manager.devAssDir();
    if (!dev.exists()) return out;

    // Iterate through all *.dadescr files and include their content with absolute target path
    QDirIterator it(dev.absolutePath(), QStringList()<<"*.dadescr", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()){
        QString descrPath = it.next();
        QFile f(descrPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QString descr = QString::fromUtf8(f.readAll());
        f.close();

        // Derive original file path (without .dadescr) relative to project root
        QString rel = dev.relativeFilePath(descrPath);
        rel.chop(QString(".dadescr").size());
        QString absTarget = QDir(m_projectRoot).filePath(rel);

        out += "==== " + QDir::toNativeSeparators(absTarget) + " ====\n";
        out += descr.trimmed() + "\n\n";
    }
    return out.trimmed();
}

QString MainWindow::getContextChainText() const
{
    if (m_contextManager) {
        return m_contextManager->generateContextText();
    }
    return QString();
}

QString MainWindow::getAllCommandsText() const
{
    if (m_contextManager) {
        // Generate text from ALL commands, not just active ones
        QStringList contextParts;
        QList<CommandContextInfo> allCommands = m_contextManager->getAllCommands();

        for (const auto& cmdInfo : allCommands) {
            QString commandText = cmdInfo.result;
            if (commandText.isEmpty()) {
                // If no specific result, use description as fallback
                commandText = cmdInfo.description;
            }

            if (!commandText.isEmpty()) {
                QString status = "";
                switch (cmdInfo.state) {
                    case CommandState::Active:
                        status = " [ACTIVE]";
                        break;
                    case CommandState::Invalidated:
                        status = QString(" [INVALIDATED: %1]").arg(cmdInfo.reason);
                        break;
                    case CommandState::Removed:
                        status = " [REMOVED]";
                        break;
                }
                contextParts << commandText + status;
            }
        }
        return contextParts.join("\n\n");
    }
    return QString();
}

// Summarizes web search results with the selected model.
QString MainWindow::summarizeSearchResults(const QString& content, const QString& query)
{
    const QString trimmedContent = content.trimmed();
    if (trimmedContent.isEmpty()) {
        return QStringLiteral("The search results contain no text to summarize.");
    }

    if (m_model.selectedModel().isEmpty()) {
        qCWarning(logUI) << "[MainWindow] Summarization skipped: LLM model not configured";
        return QStringLiteral("No model is configured, so the results could not be summarized.");
    }

    QString effectiveQuery = query.trimmed();
    if (effectiveQuery.isEmpty())
        effectiveQuery = QStringLiteral("(empty search query)");

    QString contentForPrompt = trimmedContent;
    QString truncationNote;
    constexpr int kMaxContextChars = 12000;
    if (contentForPrompt.size() > kMaxContextChars) {
        contentForPrompt = contentForPrompt.left(kMaxContextChars);
        truncationNote = QStringLiteral("\n[Content truncated to %1 characters]").arg(kMaxContextChars);
    }

    const QString systemPrompt = QStringLiteral(
        "You are a developer's assistant. Write a short, structured summary of the search results below. "
        "Give the key findings, links to the sources and recommended actions. "
        "Answer in the language of the user's request and do not make up facts."
    );
    const QString userPrompt = QStringLiteral(
        "Query: %1\n"
        "Search results:\n%2%3\n\n"
        "Write the final summary (3-4 points or short paragraphs) with the key facts, links and possible actions."
    ).arg(effectiveQuery, contentForPrompt, truncationNote);

    qCDebug(logUI) << "[MainWindow] Summarizing SEARCH_WEB results for query:" << effectiveQuery
             << "context length:" << contentForPrompt.size();

    QString summary = m_model.generateWithSystem(userPrompt, systemPrompt).trimmed();
    const QString errorPrefix = QStringLiteral("[Ollama error]");
    if (summary.startsWith(errorPrefix)) {
        qCWarning(logUI) << "[MainWindow] LLM error during search summary:" << summary;
        summary = QStringLiteral("The model returned an error: %1").arg(summary.mid(errorPrefix.length()).trimmed());
    } else if (summary.isEmpty()) {
        qCWarning(logUI) << "[MainWindow] LLM returned an empty summary for SEARCH_WEB";
        summary = QStringLiteral("The model returned no summary.");
    }

    return summary;
}


QString MainWindow::getLastUserMessages() const
{
    QString result = m_lastUserMessages.join("\n");
    qCDebug(logUI) << "[getLastUserMessages] Returning" << m_lastUserMessages.size() << "messages, total length:" << result.length();
    if (m_lastUserMessages.size() > 0) {
        qCDebug(logUI) << "[getLastUserMessages] First message:" << m_lastUserMessages.first();
        qCDebug(logUI) << "[getLastUserMessages] Last message:" << m_lastUserMessages.last();
    }
    return result;
}

void MainWindow::addCommandToContext(Command* cmd, const QString& result)
{
    if (!m_contextManager || !cmd) {
        qCDebug(logUI) << "[MainWindow] Context manager is null or cmd is null, skipping context addition";
        return;
    }

    qCDebug(logUI) << "[MainWindow] Adding command" << cmd->name() << "to context with result length:" << result.length();

    // If we're in a sequence mode, add to the current sequence, otherwise add directly
    if (m_sequenceMode && !m_currentSeqId.isEmpty()) {
        qCDebug(logUI) << "[MainWindow] Adding command to sequence" << m_currentSeqId;
        m_contextManager->addCommandToSequence(cmd, m_currentSeqId, result);
    } else {
        qCDebug(logUI) << "[MainWindow] Adding command directly to context manager";
        // The ContextManager will handle duplicate detection internally
        m_contextManager->addCommand(cmd, result);
    }
}

void MainWindow::updateContextStatusDisplay()
{
    if (!m_contextCommandsList || !m_contextManager) {
        return;
    }

    m_contextCommandsList->clear();

    // Get all sequences from context manager
    QList<CommandSequenceInfo> allSequences = m_contextManager->getCommandSequences();

    if (allSequences.isEmpty()) {
        // If no sequences, fall back to showing individual commands as before
        QList<CommandContextInfo> allCommands = m_contextManager->getAllCommands();

        for (const auto& cmdInfo : allCommands) {
            // Create top-level item for the command
            QTreeWidgetItem* commandItem = new QTreeWidgetItem(m_contextCommandsList);

            // First column: timestamp
            commandItem->setText(0, cmdInfo.timestamp.toString("hh:mm:ss"));

            // Second column: description
            commandItem->setText(1, cmdInfo.description);

            // If this command belongs to a sequence, indicate that
            if (!cmdInfo.sequenceId.isEmpty()) {
                CommandSequenceInfo seqInfo = m_contextManager->getSequenceInfo(cmdInfo.sequenceId);
                commandItem->setText(1, QString("[%1] %2").arg(seqInfo.name).arg(cmdInfo.description));
            }

            switch (cmdInfo.state) {
                case CommandState::Active: {
                    // Active commands in context shown in green
                    commandItem->setBackground(0, QColor(200, 255, 200)); // Light green
                    commandItem->setBackground(1, QColor(200, 255, 200)); // Light green
                    commandItem->setForeground(0, Qt::darkGreen);
                    commandItem->setForeground(1, Qt::darkGreen);
                    commandItem->setToolTip(0, tr("Active in context"));
                    commandItem->setToolTip(1, tr("Active in context"));
                    break;
                }
                case CommandState::Invalidated: {
                    // Invalidated commands shown in red with reason
                    commandItem->setBackground(0, QColor(255, 200, 200)); // Light red
                    commandItem->setBackground(1, QColor(255, 200, 200)); // Light red
                    commandItem->setForeground(0, Qt::red);
                    commandItem->setForeground(1, Qt::red);
                    QString tooltip = tr("Invalidated: %1").arg(cmdInfo.reason);
                    commandItem->setToolTip(0, tooltip);
                    commandItem->setToolTip(1, tooltip);
                    break;
                }
                case CommandState::Removed: {
                    // Removed commands shown in gray
                    commandItem->setBackground(0, QColor(200, 200, 200)); // Light gray
                    commandItem->setBackground(1, QColor(200, 200, 200)); // Light gray
                    commandItem->setForeground(0, Qt::gray);
                    commandItem->setForeground(1, Qt::gray);
                    commandItem->setToolTip(0, tr("Command was removed from context"));
                    commandItem->setToolTip(1, tr("Command was removed from context"));
                    break;
                }
            }

            // Store command ID in userData for potential future use
            commandItem->setData(0, Qt::UserRole, cmdInfo.id);

            // Add result as child item if it exists
            if (!cmdInfo.result.isEmpty()) {
                QTreeWidgetItem* resultItem = new QTreeWidgetItem(commandItem);
                resultItem->setText(0, tr("Result:"));
                resultItem->setText(1, cmdInfo.result);
                resultItem->setBackground(0, QColor(240, 240, 240)); // Light gray background for result header
                resultItem->setBackground(1, QColor(245, 245, 245)); // Light gray background
                resultItem->setForeground(0, Qt::darkBlue);
                resultItem->setForeground(1, Qt::darkBlue);

                // Format the result in a more readable way if it's long
                QStringList resultLines = cmdInfo.result.split('\n');
                if (resultLines.size() > 10) {
                    // Show first 10 lines and indicate more
                    QStringList shortResult = resultLines.mid(0, 10);
                    shortResult << tr("... (click to expand)");
                    resultItem->setText(1, shortResult.join('\n'));
                } else {
                    resultItem->setText(1, cmdInfo.result);
                }
            }
        }
    } else {
        // Show commands organized by sequences
        for (const auto& seqInfo : allSequences) {
            // Create top-level item for the sequence
            QTreeWidgetItem* seqItem = new QTreeWidgetItem(m_contextCommandsList);

            // First column: timestamp
            seqItem->setText(0, seqInfo.timestamp.toString("hh:mm:ss"));

            // Second column: sequence name with description
            seqItem->setText(1, tr("SEQUENCE: %1").arg(seqInfo.name));
            if (!seqInfo.description.isEmpty()) {
                seqItem->setToolTip(1, seqInfo.description);
            }

            // Set different color for sequence items (to distinguish from commands)
            seqItem->setBackground(0, QColor(255, 235, 205)); // Light orange background for sequences
            seqItem->setBackground(1, QColor(255, 235, 205)); // Light orange background
            seqItem->setForeground(0, Qt::darkBlue);
            seqItem->setForeground(1, Qt::darkBlue);

            // Store sequence ID in userData for potential future use
            seqItem->setData(0, Qt::UserRole, seqInfo.id);

            // Get commands in this sequence and add them as child items
            QList<CommandContextInfo> commandsInSequence = m_contextManager->getCommandsInSequence(seqInfo.id);
            for (const auto& cmdInfo : commandsInSequence) {
                QTreeWidgetItem* commandItem = new QTreeWidgetItem(seqItem);

                // First column: timestamp
                commandItem->setText(0, cmdInfo.timestamp.toString("hh:mm:ss"));

                // Second column: command description
                commandItem->setText(1, cmdInfo.description);

                switch (cmdInfo.state) {
                    case CommandState::Active: {
                        // Active commands in context shown in green
                        commandItem->setBackground(0, QColor(200, 255, 200)); // Light green
                        commandItem->setBackground(1, QColor(200, 255, 200)); // Light green
                        commandItem->setForeground(0, Qt::darkGreen);
                        commandItem->setForeground(1, Qt::darkGreen);
                        commandItem->setToolTip(0, tr("Active in context"));
                        commandItem->setToolTip(1, tr("Active in context"));
                        break;
                    }
                    case CommandState::Invalidated: {
                        // Invalidated commands shown in red with reason
                        commandItem->setBackground(0, QColor(255, 200, 200)); // Light red
                        commandItem->setBackground(1, QColor(255, 200, 200)); // Light red
                        commandItem->setForeground(0, Qt::red);
                        commandItem->setForeground(1, Qt::red);
                        QString tooltip = tr("Invalidated: %1").arg(cmdInfo.reason);
                        commandItem->setToolTip(0, tooltip);
                        commandItem->setToolTip(1, tooltip);
                        break;
                    }
                    case CommandState::Removed: {
                        // Removed commands shown in gray
                        commandItem->setBackground(0, QColor(200, 200, 200)); // Light gray
                        commandItem->setBackground(1, QColor(200, 200, 200)); // Light gray
                        commandItem->setForeground(0, Qt::gray);
                        commandItem->setForeground(1, Qt::gray);
                        commandItem->setToolTip(0, tr("Command was removed from context"));
                        commandItem->setToolTip(1, tr("Command was removed from context"));
                        break;
                    }
                }

                // Store command ID in userData for potential future use
                commandItem->setData(0, Qt::UserRole, cmdInfo.id);

                // Add result as child item if it exists
                if (!cmdInfo.result.isEmpty()) {
                    QTreeWidgetItem* resultItem = new QTreeWidgetItem(commandItem);
                    resultItem->setText(0, tr("Result:"));
                    resultItem->setText(1, cmdInfo.result);
                    resultItem->setBackground(0, QColor(240, 240, 240)); // Light gray background for result header
                    resultItem->setBackground(1, QColor(245, 245, 245)); // Light gray background
                    resultItem->setForeground(0, Qt::darkBlue);
                    resultItem->setForeground(1, Qt::darkBlue);

                    // Format the result in a more readable way if it's long
                    QStringList resultLines = cmdInfo.result.split('\n');
                    if (resultLines.size() > 10) {
                        // Show first 10 lines and indicate more
                        QStringList shortResult = resultLines.mid(0, 10);
                        shortResult << tr("... (click to expand)");
                        resultItem->setText(1, shortResult.join('\n'));
                    } else {
                        resultItem->setText(1, cmdInfo.result);
                    }
                }
            }
        }
    }

    // Expand only the top-level sequence/command items, keeping result items collapsed by default
    // This shows the structure but keeps the display compact
    for (int i = 0; i < m_contextCommandsList->topLevelItemCount(); ++i) {
        QTreeWidgetItem* topLevelItem = m_contextCommandsList->topLevelItem(i);
        topLevelItem->setExpanded(true);

        // Collapse any child items (results) by default, so they can be expanded when needed
        for (int j = 0; j < topLevelItem->childCount(); ++j) {
            QTreeWidgetItem* childItem = topLevelItem->child(j);
            // Keep child items (like results) collapsed by default
            childItem->setExpanded(false);
        }
    }

    if (m_contextGraphWidget) {
        m_contextGraphWidget->setGraphData(
            m_contextManager->getContextGraphNodes(),
            m_contextManager->activeContextGraphNodeId());
    }
}

void MainWindow::onFileChanged(const QString &path)
{
    qCDebug(logUI) << "[MainWindow] File changed externally:" << path;

    // Remove the file from the watcher to avoid immediate re-triggering
    m_fileWatcher->removePath(path);

    // Add a small delay to allow file operations to complete
    QTimer::singleShot(100, this, [this, path]() {
        // Re-add the file to the watcher
        if (QFileInfo::exists(path)) {
            m_fileWatcher->addPath(path);
        }

        // Check if this file has context in our tracking
        if (m_fileToContextCommands.contains(path)) {
            qCDebug(logUI) << "[MainWindow] Clearing context for file due to external change:" << path;

            // Clear contexts related to this file
            m_contextCache.clearContextForPath(path);

            // Also remove from file-to-commands mapping
            m_fileToContextCommands.remove(path);
        }

        // Inform ContextManager so command history is cleaned as well
        if (m_contextManager) {
            m_contextManager->cleanupForPath(path, QString("Context invalidated: %1 changed externally").arg(path));
        }
    });
}

void MainWindow::runTemplateRequested(const QString &templatePath, const QString &text)
{
    // This method is called when a specific template is selected from the combobox
    // We need to run the selected template with the provided text as the user query

    qCDebug(logUI) << "[runTemplateRequested] Template path:" << templatePath;
    qCDebug(logUI) << "[runTemplateRequested] Text:" << text;
    resetSequenceStabilityState();
    if (!m_preserveActiveImagesForNextRun) {
        m_activeImagesBase64.clear();
        m_activeImagePaths.clear();
        m_activeImageSummaries.clear();
    }
    m_preserveActiveImagesForNextRun = false;
    m_sequenceEditor->onChatTemplateSelectionChanged(templatePath);
    if (m_contextManager)
        m_contextManager->observeUserQuery(text);
    appendUserQuestionToResultDisplay(text);

    // First, refresh to ensure we have the latest templates and sequences
    m_templateMgr.refresh();
    refreshAllSequences(); // Refresh sequences including those in templates
    ensureDefaultSequence();

    // Find the template by path
    const QMap<QString, SolutionTemplate>& templates = m_templateMgr.templates();
    SolutionTemplate selectedTemplate;
    QString foundTemplatePath;

    for (auto it = templates.constBegin(); it != templates.constEnd(); ++it) {
        if (it.key() == templatePath) {
            selectedTemplate = it.value();
            foundTemplatePath = it.key();
            break;
        }
    }

    if (foundTemplatePath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Selected template not found: %1").arg(templatePath));
        return;
    }

    // Get the main sequence for this template
    QString mainSequencePath = selectedTemplate.mainSequencePath();

    // First, try to use the main sequence path as specified in the template file,
    // resolved relative to the template directory
    if (!mainSequencePath.isEmpty()) {
        QString resolvedMainSeqPath = QDir(foundTemplatePath).absoluteFilePath(mainSequencePath);
        if (QFile::exists(resolvedMainSeqPath)) {
            mainSequencePath = resolvedMainSeqPath;
        } else {
            // If the main sequence specified in template doesn't exist, fall back to first sequence
            auto sequenceNames = selectedTemplate.sequences();
            if (!sequenceNames.isEmpty()) {
                // Build the path to the first sequence file in the template directory
                mainSequencePath = QDir(foundTemplatePath).absoluteFilePath(sequenceNames[0]);
            } else {
                mainSequencePath = ""; // No sequences available
            }
        }
    } else {
        // If no main sequence is set in template, use the first available sequence
        auto sequenceNames = selectedTemplate.sequences();
        if (!sequenceNames.isEmpty()) {
            // Build the path to the first sequence file in the template directory
            mainSequencePath = QDir(foundTemplatePath).absoluteFilePath(sequenceNames[0]);
        }
    }

    if (mainSequencePath.isEmpty() || !QFile::exists(mainSequencePath)) {
        QMessageBox::warning(this, tr("Error"), tr("No valid sequence found for template: %1").arg(selectedTemplate.name()));
        return;
    }

    // Load the selected sequence
    PromptSequence seq;
    QFile seqFile(mainSequencePath);
    if (seqFile.open(QIODevice::ReadOnly)) {
        QByteArray data = seqFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            seq = PromptSequence::fromJson(doc.object());
        }
        seqFile.close();
    }

    if (seq.name().isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Could not load the sequence for template: %1").arg(selectedTemplate.name()));
        return;
    }

    // Set up the sequence to run and store the user query for use in templates
    m_currentSeq = seq;
    m_currentStepIdx = 0;
    m_sequenceMode = true;
    m_currentSeqPath = mainSequencePath; // Track the sequence path for related sequence lookup
    m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description()); // Start tracking this sequence
    m_agentRepairCountThisStep = 0;
    m_agentRepairStepIdx = -1;
    m_agentRepairSeqId.clear();
    m_lastUserMessage = text; // Save the user's query for resolution in prompts

    m_sequenceEditor->updateSequenceGraphHighlight();  // Update the graph highlighting
    qCDebug(logUI) << "[runTemplateRequested] About to start task timer for template execution";
    m_statusController->startTask();
    m_sequenceRunner->runNextStep();  // Start the sequence immediately
}


void MainWindow::runSimpleChat(const QString &text)
{
    // This method is called when "Simple chat" is selected - direct chat mode
    // Send the message directly to the model in streaming mode without templates/sequences

    qCDebug(logUI) << "[runSimpleChat] Text:" << text;
    resetSequenceStabilityState();

    if (m_contextManager)
        m_contextManager->observeUserQuery(text);

    // Display user message in chat
    m_chat->appendUserMessage(text);
    appendUserQuestionToResultDisplay(text);

    // Start streaming response directly without any system context or templates
    m_streamBuf.clear();
    m_model.startStream(text);

    // Update history
    // Add to main history
    m_history.push_back(qMakePair(QString("user"), text));
    // Also add to separate user messages storage (for LAST_USER_MESSAGES resolver)
    m_lastUserMessages.append(text);
    // Keep only last N messages
    while (m_lastUserMessages.size() > MAX_USER_MESSAGES) {
        m_lastUserMessages.removeFirst();
    }

    // Log the request
    int tokens = m_promptMgr.tokenCount(text);
    log(QString("Sending simple chat request (%1 tokens) → %2").arg(tokens).arg(text));
}

static QString buildImagesNoteText(const QStringList &imagePaths, const QStringList &imageSummaries)
{
    if (imagePaths.isEmpty())
        return QString();

    QString out = "\n\n[Images]\n";
    for (int i = 0; i < imagePaths.size(); ++i) {
        const QString fileName = QFileInfo(imagePaths[i]).fileName();
        const QString summary = (i < imageSummaries.size()) ? imageSummaries[i].trimmed() : QString();
        if (!summary.isEmpty()) {
            out += QString("- %1: %2\n").arg(fileName, summary);
        } else {
            out += QString("- %1\n").arg(fileName);
        }
    }
    return out.trimmed();
}

static void addImagesToContextManager(ContextManager *ctx,
                                     const QStringList &imagePaths,
                                     const QStringList &imageSummaries)
{
    if (!ctx || imagePaths.isEmpty())
        return;

    class InlineImageContextCommand final : public Command {
    public:
        InlineImageContextCommand(const QString &name,
                                  const QJsonObject &params,
                                  const QString &desc,
                                  const QString &result)
            : m_name(name), m_params(params), m_desc(desc), m_result(result) {}

        QString name() const override { return m_name; }
        QJsonObject params() const override { return m_params; }
        QString description() const override { return m_desc; }
        bool apply(DevAssManager*, QString*) override { return true; }
        QJsonObject toJson() const override {
            QJsonObject o;
            o["command"] = m_name;
            o["params"] = m_params;
            o["description"] = m_desc;
            return o;
        }
        QString getResult() const override { return m_result; }

    private:
        QString m_name;
        QJsonObject m_params;
        QString m_desc;
        QString m_result;
    };

    for (int i = 0; i < imagePaths.size(); ++i) {
        const QString path = imagePaths[i];
        const QString fileName = QFileInfo(path).fileName();
        const QString summary = (i < imageSummaries.size()) ? imageSummaries[i].trimmed() : QString();

        QJsonObject params;
        params["path"] = path;
        if (!summary.isEmpty())
            params["summary"] = summary;

        const QString result = summary.isEmpty()
                                   ? QString("=== IMAGE: %1 ===\n%2").arg(fileName, path)
                                   : QString("=== IMAGE: %1 ===\n%2").arg(fileName, summary);
        InlineImageContextCommand cmd("USER_IMAGE", params, "User attached an image", result);
        ctx->addCommand(&cmd, result);
    }
}

void MainWindow::sendUserMessageWithImages(const QString &text,
                                          const QStringList &imagePaths,
                                          const QStringList &imagesBase64,
                                          const QStringList &imageSummaries)
{
    m_activeImagesBase64 = imagesBase64;
    m_activeImagePaths = imagePaths;
    m_activeImageSummaries = imageSummaries;
    addImagesToContextManager(m_contextManager, imagePaths, imageSummaries);
    sendUserMessage(text);
}

void MainWindow::runTemplateRequestedWithImages(const QString &templatePath,
                                               const QString &text,
                                               const QStringList &imagePaths,
                                               const QStringList &imagesBase64,
                                               const QStringList &imageSummaries)
{
    m_activeImagesBase64 = imagesBase64;
    m_activeImagePaths = imagePaths;
    m_activeImageSummaries = imageSummaries;
    m_preserveActiveImagesForNextRun = true;
    addImagesToContextManager(m_contextManager, imagePaths, imageSummaries);
    runTemplateRequested(templatePath, text);
}

void MainWindow::runSimpleChatWithImages(const QString &text,
                                        const QStringList &imagePaths,
                                        const QStringList &imagesBase64,
                                        const QStringList &imageSummaries)
{
    qCDebug(logUI) << "[runSimpleChatWithImages] Text:" << text << "images:" << imagePaths.size();

    addImagesToContextManager(m_contextManager, imagePaths, imageSummaries);

    const QString note = buildImagesNoteText(imagePaths, imageSummaries);
    const QString historyText = note.isEmpty() ? text : (text + note);

    if (m_contextManager)
        m_contextManager->observeUserQuery(historyText);

    m_chat->appendUserMessage(note.isEmpty() ? text : (text + QStringLiteral("\n") + tr("[Images attached: %n]", nullptr, imagePaths.size())));
    appendUserQuestionToResultDisplay(historyText);

    m_streamBuf.clear();
    m_model.startStreamWithSystemAndImages(text, QString(), imagesBase64, QVariantMap());

    m_history.push_back(qMakePair(QString("user"), historyText));
    m_lastUserMessages.append(historyText);
    while (m_lastUserMessages.size() > MAX_USER_MESSAGES) {
        m_lastUserMessages.removeFirst();
    }

    int tokens = m_promptMgr.tokenCount(historyText);
    log(QString("Sending simple chat request (%1 tokens) -> %2").arg(tokens).arg(historyText));
}

ResultDisplayWidget::ResultEntryMetadata MainWindow::collectContextMetadataSince(const QDateTime &since) const
{
    ResultDisplayWidget::ResultEntryMetadata metadata;
    if (!m_contextManager || !since.isValid())
        return metadata;

    QList<CommandContextInfo> relevantCommands;
    QSet<QString> relevantIds;
    for (const auto &cmd : m_contextManager->getAllCommands()) {
        if (cmd.timestamp >= since) {
            relevantCommands << cmd;
            relevantIds.insert(cmd.id);
        }
    }

    if (relevantCommands.isEmpty())
        return metadata;

    metadata.commands = relevantCommands;

    for (const auto &seq : m_contextManager->getCommandSequences()) {
        bool include = seq.timestamp >= since;
        if (!include) {
            for (const QString &cmdId : seq.commandIds) {
                if (relevantIds.contains(cmdId)) {
                    include = true;
                    break;
                }
            }
        }
        if (include) {
            metadata.sequences << seq;
        }
    }

    return metadata;
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    qCDebug(logUI) << "[MainWindow] closeEvent called";

    // Save sequence graph layout before closing if it exists and is initialized
    if (m_seqGraphWindow) {
        qCDebug(logUI) << "[MainWindow] Saving sequence graph layout";
        m_seqGraphWindow->saveLayout();
    } else {
        qCDebug(logUI) << "[MainWindow] No sequence graph window to save";
    }

    // Clean up the sequence graph window
    if (m_seqGraphWindow) {
        delete m_seqGraphWindow;
        m_seqGraphWindow = nullptr;
    }

    QMainWindow::closeEvent(event);
}



void MainWindow::updateChatToggleButton(bool visible)
{
    if (chatToggleBtn) {
        chatToggleBtn->setText(visible ? tr("Hide chat") : tr("Show chat"));
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Service interface implementations (delegation to existing members)
// ═══════════════════════════════════════════════════════════════════════

// ─── IContextService ──────────────────────────────────────────────────
void MainWindow::setFullFileContext(const QString& path, const QStringList& lines) { m_contextCache.setFullFileContext(path, lines); }
void MainWindow::addRegionContext(const QString& path, int startLine, const QStringList& lines) { m_contextCache.addRegionContext(path, startLine, lines); }
void MainWindow::clearContextForPath(const QString& path) { m_contextCache.clearContextForPath(path); }
void MainWindow::clearAll() { m_contextCache.clearAll(); }
QString MainWindow::contextContent() const { return m_contextCache.contextContent(); }
void MainWindow::appendConsoleLog(const QString& log) { m_contextCache.appendConsoleLog(log); }
void MainWindow::removeConsoleLogsWithPrefix(const QString& prefix) { m_contextCache.removeConsoleLogsWithPrefix(prefix); }
void MainWindow::appendExternalSource(const QString& source) { m_contextCache.appendExternalSource(source); }
void MainWindow::clearConsoleLogs() { m_contextCache.clearConsoleLogs(); }
void MainWindow::clearExternalSources() { m_contextCache.clearExternalSources(); }
QStringList MainWindow::consoleLogs() const { return m_contextCache.consoleLogs(); }
QStringList MainWindow::externalSources() const { return m_contextCache.externalSources(); }
QString MainWindow::formatNumberedLines(const QStringList& lines, int startLine) const { return m_contextCache.formatNumberedLines(lines, startLine); }

bool MainWindow::hasContextManager() const { return m_contextManager != nullptr; }
void MainWindow::observeUserQuery(const QString& query) { if (m_contextManager) m_contextManager->observeUserQuery(query); }
void MainWindow::addCommandToContext(int cmdIndex, const QString& description, const QString& result) { Q_UNUSED(cmdIndex) Q_UNUSED(description) Q_UNUSED(result) }
QString MainWindow::generateContextText() const { return m_contextManager ? m_contextManager->generateContextText() : QString(); }

QList<IContextService::SeqInfo> MainWindow::getCommandSequences() const {
    if (!m_contextManager) return {};
    QList<IContextService::SeqInfo> result;
    for (const auto& s : m_contextManager->getCommandSequences()) {
        result.append({s.id, s.name, s.description, s.timestamp});
    }
    return result;
}
IContextService::SeqInfo MainWindow::getSequenceInfo(const QString& id) const {
    if (!m_contextManager) return {};
    auto s = m_contextManager->getSequenceInfo(id);
    return {s.id, s.name, s.description, s.timestamp};
}
QList<IContextService::CmdInfo> MainWindow::getAllCommands() const {
    if (!m_contextManager) return {};
    QList<IContextService::CmdInfo> result;
    for (const auto& c : m_contextManager->getAllCommands()) {
        result.append({c.id, c.description, c.result, c.sequenceId, static_cast<int>(c.state), c.reason, c.timestamp});
    }
    return result;
}
QList<IContextService::CmdInfo> MainWindow::getCommandsInSequence(const QString& seqId) const {
    if (!m_contextManager) return {};
    QList<IContextService::CmdInfo> result;
    for (const auto& c : m_contextManager->getCommandsInSequence(seqId)) {
        result.append({c.id, c.description, c.result, c.sequenceId, static_cast<int>(c.state), c.reason, c.timestamp});
    }
    return result;
}

bool MainWindow::fileHasContextCommands(const QString& path) const { return m_fileToContextCommands.contains(path); }
QSet<QString> MainWindow::getContextCommandsForFile(const QString& path) const { return m_fileToContextCommands.value(path); }
void MainWindow::clearFileToContextCommands() { m_fileToContextCommands.clear(); }
void MainWindow::insertFileContextCommand(const QString& path, const QString& cmd) { m_fileToContextCommands[path].insert(cmd); }
void MainWindow::clearShellCommandOutputs() { m_shellCommandOutputs.clear(); }
void MainWindow::clearPythonScriptOutputs() { m_pythonScriptOutputs.clear(); }
void MainWindow::removeShellOutput(const QString& key) { m_shellCommandOutputs.remove(key); }
void MainWindow::removePythonOutput(const QString& key) { m_pythonScriptOutputs.remove(key); }
QFileSystemWatcher* MainWindow::fileWatcher() { return m_fileWatcher; }

// ─── IModelService ────────────────────────────────────────────────────
void MainWindow::configure(const QString& model, const QVariantMap& params) { m_model.configure(model, params); }
void MainWindow::setProvider(int provider) { m_model.setProvider(static_cast<OllamaAdapter::Provider>(provider)); }
void MainWindow::setEndpoint(const QUrl& url) { m_model.setEndpoint(url); }
QString MainWindow::selectedModel() const { return m_model.selectedModel(); }
int MainWindow::provider() const { return static_cast<int>(m_model.provider()); }
QString MainWindow::providerName() const { return m_model.providerName(); }
void MainWindow::appendAssistantChunk(const QString& chunk) { if (m_chat) m_chat->appendAssistantChunk(chunk); }
void MainWindow::refreshModelList() { if (m_chat) m_chat->refreshModelList(); }
bool MainWindow::hasChat() const { return m_chat != nullptr; }
QString& MainWindow::streamBuf() { return m_streamBuf; }
void MainWindow::appendHistory(const QString& role, const QString& content) { m_history.push_back(qMakePair(role, content)); }
void MainWindow::appendLastUserMessage(const QString& msg) {
    m_lastUserMessages.append(msg);
    while (m_lastUserMessages.size() > MAX_USER_MESSAGES) m_lastUserMessages.removeFirst();
}
int MainWindow::maxUserMessages() const { return MAX_USER_MESSAGES; }
OllamaAdapter* MainWindow::modelPtr() { return &m_model; }
void MainWindow::setLastUserMessage(const QString& msg) { m_lastUserMessage = msg; }
QString MainWindow::lastUserMessage() const { return m_lastUserMessage; }
QString MainWindow::selectedAgentModel() const { return m_agentModel.selectedModel(); }
void MainWindow::configureAgentModel(const QString& model, const QVariantMap& params) { m_agentModel.configure(model, params); }
void MainWindow::setAgentProvider(int provider) { m_agentModel.setProvider(static_cast<OllamaAdapter::Provider>(provider)); }
void MainWindow::setAgentEndpoint(const QUrl& url) { m_agentModel.setEndpoint(url); }

// ─── IProjectService ──────────────────────────────────────────────────
QString MainWindow::projectRoot() const { return m_projectRoot; }
void MainWindow::setProjectRoot(const QString& path) { m_projectRoot = path; }
QStringList MainWindow::indexedFiles() const { return m_index.files(); }
void MainWindow::setIndexRoot(const QString& path) { m_index.setRoot(path); }
void MainWindow::loadDevManager() { m_dev_manager.load(); }
QString MainWindow::devAssDirPath() const { return m_dev_manager.devAssDir().absolutePath(); }
FileDescription* MainWindow::findFileDescription(const QString& path) { return m_dev_manager.findFileDescription(path); }
void MainWindow::setDevProjectRoot(const QString& path) { m_dev_manager.setProjectRoot(path); }
bool MainWindow::createFolder() { return m_dev_manager.createFolder(); }
bool MainWindow::createDaproFile() { return m_dev_manager.createDaproFile(); }
bool MainWindow::generateDescrFiles() { return m_dev_manager.generateDescrFiles(); }
void MainWindow::setPromptProjectRoot(const QString& path) { m_promptMgr.setProjectRoot(path); }
void* MainWindow::promptMgr() { return static_cast<void*>(&m_promptMgr); }
QListWidget* MainWindow::filesList() { return m_files; }
void MainWindow::openProjectInIDE(const QString& path) { ide_widget->openProjectbyPath(path); }
void MainWindow::setRootEditText(const QString& path) { m_rootEdit->setText(path); }
void MainWindow::emitMissingFilesUpdated(const QSet<QString>& paths) { emit MainWindow::missingFilesUpdated(paths); }
// rescan() already implemented at line 1005 (serves as slot + interface impl)
DevAssManager* MainWindow::devManagerPtr() { return &m_dev_manager; }

// ─── IUIUpdater ───────────────────────────────────────────────────────
// log() already implemented at line 1700
void MainWindow::statusBarMessage(const QString& msg) { statusBar()->showMessage(msg); }
// appendUserQuestionToResultDisplay() already implemented at line 2477
void MainWindow::appendConsoleToResultDisplay(const QString& block) { m_sequenceRunner->appendResultToUi(tr("Console"), block); }
void MainWindow::appendExternalToResultDisplay(const QString& block) { m_sequenceRunner->appendResultToUi(tr("External"), block); }
QTreeWidget* MainWindow::contextCommandsList() { return m_contextCommandsList; }

// ─── ISequenceService ─────────────────────────────────────────────────
bool MainWindow::isStopRequested() const { return m_stopRequested; }
void MainWindow::setStopRequested(bool v) { m_stopRequested = v; }
bool MainWindow::isSequenceMode() const { return m_sequenceMode; }
// resetSequenceStabilityState() already implemented at line 1560
// runSequenceDialog() already implemented earlier (serves as slot + interface impl)
void MainWindow::stopTask() { if (m_statusController) m_statusController->stopTask(); }
PromptSequenceManager* MainWindow::seqMgrPtr() { return &m_seqMgr; }
PromptSequence* MainWindow::currentSequencePtr() { return &m_currentSeq; }
int MainWindow::currentStepIndex() const { return m_currentStepIdx; }
void MainWindow::setCurrentStepIndex(int idx) { m_currentStepIdx = idx; }
QString MainWindow::currentSeqPath() const { return m_currentSeqPath; }
void MainWindow::setCurrentSeqPath(const QString& path) { m_currentSeqPath = path; }
QString MainWindow::currentSeqId() const { return m_currentSeqId; }
void MainWindow::setCurrentSeqId(const QString& id) { m_currentSeqId = id; }
QString MainWindow::currentTemplatePath() const { return m_currentTemplatePath; }
void MainWindow::setCurrentTemplatePath(const QString& path) { m_currentTemplatePath = path; }
// Agent mode (a planning call and an execution call per step) is off by default: it doubles
// the model calls and turns streaming off. Settings > System switches it.
bool MainWindow::useAgentPipeline() const { return QSettings().value("agent/enabled", false).toBool(); }
void MainWindow::setUseAgentPipeline(bool v) { QSettings().setValue("agent/enabled", v); }
int MainWindow::agentRepairCount() const { return m_agentRepairCountThisStep; }
void MainWindow::setAgentRepairCount(int v) { m_agentRepairCountThisStep = v; }
int MainWindow::agentMaxRepairs() const { return m_agentMaxRepairsPerStep; }
bool MainWindow::inAgentAutoRepair() const { return m_inAgentAutoRepair; }
void MainWindow::setInAgentAutoRepair(bool v) { m_inAgentAutoRepair = v; }
int MainWindow::agentRepairStepIdx() const { return m_agentRepairStepIdx; }
void MainWindow::setAgentRepairStepIdx(int v) { m_agentRepairStepIdx = v; }
QString MainWindow::agentRepairSeqId() const { return m_agentRepairSeqId; }
void MainWindow::setAgentRepairSeqId(const QString& id) { m_agentRepairSeqId = id; }
QList<Change>& MainWindow::pendingChanges() { return m_pending; }
ChangeApplier* MainWindow::applierPtr() { return &m_applier; }
// collectContextMetadataSince() already implemented at line 2482

// ─── IGraphService ────────────────────────────────────────────────────
void MainWindow::showGraphWindow() {
    m_graphWindow->show(); m_graphWindow->raise(); m_graphWindow->activateWindow();
}
void MainWindow::showLightRagWindow() {
    if (!m_lightRagWidget) { m_lightRagWidget = new LightRagWidget(); m_lightRagWidget->setAttribute(Qt::WA_DeleteOnClose, true); }
    m_lightRagWidget->show(); m_lightRagWidget->raise(); m_lightRagWidget->activateWindow();
}
SequenceGraphWidget* MainWindow::sequenceGraphWindow() { return m_seqGraphWindow; }

// ─── ISettingsService ─────────────────────────────────────────────────
void MainWindow::showSettings() { if (m_llmSettingsWindow) m_llmSettingsWindow->show(); }
void MainWindow::hideSettings() { if (m_llmSettingsWindow) m_llmSettingsWindow->hide(); }
bool MainWindow::isSettingsVisible() const { return m_llmSettingsWindow && m_llmSettingsWindow->isVisible(); }
void MainWindow::reloadSettingsFromConfig() { if (m_llmSettingsWindow) m_llmSettingsWindow->reloadFromConfig(); }
void MainWindow::setSettingsButtonText(const QString& text) { if (settingsBtn) settingsBtn->setText(text); }
QString MainWindow::providerString() const { return m_llmSettingsWindow ? m_llmSettingsWindow->provider() : QString(); }
QString MainWindow::endpointString() const { return m_llmSettingsWindow ? m_llmSettingsWindow->endpoint() : QString(); }
QVariantMap MainWindow::paramsMap() const { return m_llmSettingsWindow ? m_llmSettingsWindow->params() : QVariantMap(); }

// ─── IContextService additional accessors ─────────────────────────────
ContextManager* MainWindow::contextManagerPtr() { return m_contextManager; }
ContextCacheManager* MainWindow::contextCachePtr() { return &m_contextCache; }
QMap<QString, QSet<QString>>& MainWindow::fileToContextCommands() { return m_fileToContextCommands; }
QMap<QString, QString>& MainWindow::shellCommandOutputs() { return m_shellCommandOutputs; }
QMap<QString, QString>& MainWindow::pythonScriptOutputs() { return m_pythonScriptOutputs; }

// ─── IModelService additional accessors ────────────────────────────────
QList<QPair<QString, QString>>& MainWindow::history() { return m_history; }
QList<QString>& MainWindow::lastUserMessages() { return m_lastUserMessages; }
QString& MainWindow::lastAssistantMessage() { return m_lastAssistantMessage; }
QString& MainWindow::currentPrompt() { return m_currentPrompt; }
QStringList& MainWindow::activeImagesBase64() { return m_activeImagesBase64; }
OllamaAdapter* MainWindow::agentModelPtr() { return &m_agentModel; }

// ─── IUIUpdater additional accessors ───────────────────────────────────
ResultDisplayWidget* MainWindow::resultDisplayWidget() { return m_resultDisplayWidget; }
QDateTime& MainWindow::lastQuestionTimestamp() { return m_lastQuestionTimestamp; }
QComboBox* MainWindow::confirmModeComboBox() { return confirmModeCombo; }

// ─── ISequenceService additional accessors ─────────────────────────────
PromptSequence& MainWindow::currentSeq() { return m_currentSeq; }
QStringList& MainWindow::prevSteps() { return m_prevSteps; }
void MainWindow::setSequenceMode(bool v) { m_sequenceMode = v; }
int& MainWindow::agentRepairCountThisStep() { return m_agentRepairCountThisStep; }

// ─── ITemplateService ──────────────────────────────────────────────────
SolutionTemplateManager* MainWindow::templateMgrPtr() { return &m_templateMgr; }

// ─── IStatusControllerService ──────────────────────────────────────────
TaskStatusController* MainWindow::statusControllerPtr() { return m_statusController; }

// ─── ITTSService ───────────────────────────────────────────────────────
TtsController* MainWindow::ttsControllerPtr() { return m_ttsController; }

