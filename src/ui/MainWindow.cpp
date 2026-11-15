#include "ui/MainWindow.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include "core/util/AppConfig.h"
#include "ui/ContextGraphWidget.h"
#include "ui/ModelSettingsPanel.h"
#include "ui/ResultDisplayWidget.h"
#include "ui/controllers/ContextTreeView.h"
#include "ui/controllers/SequenceEditorController.h"
#include "ui/controllers/TaskStatusController.h"
#include "ui/controllers/TtsController.h"
#include "ui/controllers/UiBuilder.h"
#include "ui/dialogs/RunTemplateOrSequenceDialog.h"
#include "ui/editor/IDEWidget.h"
#include "ui/graph/GraphWidget.h"
#include "ui/graph/SequenceGraphWidget.h"
#include "ui/rag/LightRagWidget.h"
#include "ui/rag/RagWidget.h"
#include "ui/widgets/MemoryComboBox.h"
#include "ui/widgets/MemoryLineEdit.h"
#include "ui/widgets/MemorySplitter.h"
#include "ui/widgets/StyleTextEdit.h"
#include "ui/widgets/StyledButton.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_host(this)
    , m_engine(m_session, m_host)
    , m_describer(m_session.model(), m_session.project().manager())
{
    AppConfig config;
    config.load();
    m_session.applyModelSettings(config);
    m_session.loadWorkflows(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                            + QStringLiteral("/DevelopersAssistant"));

    m_status = new TaskStatusController(this);
    m_tts = new TtsController(this);
    // The strings in UiBuilder belong to the "UiBuilder" translation context, where lupdate
    // files them (-tr-function-alias tr+=tr_fn).
    UiBuilder::build(m_ui, [](const char* s) { return QCoreApplication::translate("UiBuilder", s); },
                     m_session, m_tts, this);
    m_sequences = std::make_unique<SequenceEditorController>(m_session, m_ui.m_seqGraphWindow, this);

    setWindowTitle(QStringLiteral("DevAssistant"));
    auto* tabs = new QTabWidget(this);
    tabs->addTab(m_ui.mainSplitter, tr("Assistant"));
    tabs->addTab(m_ui.m_settingsPanel, tr("Settings"));
    setCentralWidget(tabs);

    // Always at hand: undoes the assistant's last change to the project files.
    m_ui.undoBtn->setText(tr("Undo last change"));
    m_ui.undoBtn->setToolTip(tr("Reverts the last change the assistant made to the project files "
                                "(up to 50 steps back)."));
    statusBar()->addPermanentWidget(m_ui.undoBtn);
    m_ui.undoBtn->show();
    m_status->initStatusBar(statusBar());
    m_status->setModel(&m_session.model());
    showModelInStatusBar();

    connectProject();
    connectChat();
    connectEngine();
    connectSettings();
    connectTools();

    restoreLayout();
    m_sequences->refreshGraph();
    showContext();
    openProject(m_ui.m_rootEdit->text());
}

MainWindow::~MainWindow()
{
    m_engine.stop();
    m_describer.cancel();
    // The widgets refer to the session (the chat to the model, the graph to the sequences):
    // delete them while it still exists, not after it as Qt would.
    delete m_ui.m_graphWindow;  // a separate window without a parent
    delete takeCentralWidget();
}

// ---------------------------------------------------------------- wiring

void MainWindow::connectProject()
{
    Project& project = m_session.project();
    connect(&project, &Project::opened, this, [this](const QString& root) {
        m_ui.m_rootEdit->setText(root);
        m_ui.ide_widget->openProjectbyPath(root);
        if (m_documents)
            m_documents->setFolder(root);
    });
    connect(&project, &Project::scanned, this, &MainWindow::showProjectFiles);
    connect(m_ui.browseBtn, &StyledButton::clicked, this, &MainWindow::browse);
    connect(m_ui.rescanBtn, &StyledButton::clicked, &project, &Project::rescan);
    connect(m_ui.undoBtn, &StyledButton::clicked, this, &MainWindow::undoLastChange);
    connect(m_ui.descBtn, &StyledButton::clicked, this, &MainWindow::describeFiles);
    connect(&m_session.context(), &WorkingContext::changed, this, &MainWindow::showContext);
}

void MainWindow::connectChat()
{
    ChatPanel* chat = m_ui.m_chat;
    Conversation& conversation = m_session.conversation();
    connect(chat, &ChatPanel::requestSent, this, &MainWindow::handleRequest);
    connect(chat, &ChatPanel::stopRequested, &m_engine, &AgentEngine::stop);
    connect(chat, &ChatPanel::requestEdited, this, [&conversation](const QString& text) {
        conversation.setCurrentRequest(text);
    });
    connect(chat, &ChatPanel::templateSelectionChanged, this, [this](const QString& path) {
        m_sequences->setSelectedTemplate(path);
    });
    connect(chat, &ChatPanel::chatLogVisibilityChanged, this, [this](bool visible) {
        m_ui.chatToggleBtn->setText(visible ? tr("Hide chat") : tr("Show chat"));
    });
    connect(m_ui.chatToggleBtn, &StyledButton::clicked, this, [chat]() {
        chat->setChatLogVisible(!chat->isChatLogVisible());
    });
    // The editor can start a run and set the request too.
    connect(m_ui.ide_widget, &IDEWidget::signal_runSequenceDialog, this, [this]() { chooseAndRun(false); });
    connect(m_ui.ide_widget, &IDEWidget::signal_updatelastUserMessage, this, [&conversation](const QString& text) {
        conversation.setCurrentRequest(text);
    });
}

void MainWindow::connectEngine()
{
    AgentEngine& engine = m_engine;
    connect(&engine, &AgentEngine::log, this, &MainWindow::log);
    connect(&engine, &AgentEngine::answerChunk, m_ui.m_chat, &ChatPanel::appendAssistantChunk);
    connect(&engine, &AgentEngine::answerFinished, m_ui.m_chat, &ChatPanel::finishAssistantMessage);
    connect(&engine, &AgentEngine::resultReady, this, &MainWindow::showResult);
    connect(&engine, &AgentEngine::runStarted, m_status, &TaskStatusController::startTask);
    connect(&engine, &AgentEngine::runFinished, this, [this](const QString& message) {
        m_status->stopTask();
        if (!message.isEmpty()) {
            statusBar()->showMessage(message, 10000);
            log(message);
        }
    });
    connect(&engine, &AgentEngine::stepChanged, this, [this]() { m_sequences->highlightRunningSequence(); });
    connect(&engine, &AgentEngine::projectChanged, &m_session.project(), &Project::rescan);
    connect(&engine, &AgentEngine::promptSent, m_status, &TaskStatusController::setCurrentPrompt);
    connect(&engine, &AgentEngine::speak, m_tts, &TtsController::speak);

    // How much the user is asked, and agent mode: from the settings page.
    const auto applyConfirmMode = [this]() {
        m_engine.setConfirmMode(AgentEngine::confirmModeFromString(m_ui.confirmModeCombo->currentData().toString()));
    };
    connect(m_ui.confirmModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyConfirmMode);
    applyConfirmMode();
    if (m_ui.agentModeCheck) {
        m_engine.setAgentMode(m_ui.agentModeCheck->isChecked());
        connect(m_ui.agentModeCheck, &QCheckBox::toggled, &m_engine, &AgentEngine::setAgentMode);
    }
}

void MainWindow::connectSettings()
{
    connect(m_ui.m_settingsSidebar, &QListWidget::currentRowChanged, m_ui.m_settingsPages, &QStackedWidget::setCurrentIndex);
    connect(m_ui.settingsBtn, &StyledButton::clicked, this, [this]() {
        ModelSettingsPanel* panel = m_ui.m_llmSettingsWindow;
        const bool show = !panel->isVisible();
        if (show)
            panel->reloadFromConfig();
        panel->setVisible(show);
        m_ui.settingsBtn->setText(show ? tr("Hide LLM settings") : tr("Open LLM settings"));
    });
    connect(m_ui.m_llmSettingsWindow, &ModelSettingsPanel::saveRequested, this, &MainWindow::applyModelSettings);
    connect(m_tts, &TtsController::statusMessage, this, [this](const QString& m) { log(QStringLiteral("TTS: ") + m); });
    connect(m_tts, &TtsController::errorMessage, this, [this](const QString& e) { log(QStringLiteral("TTS error: ") + e); });
    connect(m_ui.logToggleBtn, &StyledButton::clicked, this, [this]() {
        const bool visible = !m_ui.m_log->isVisible();
        m_ui.m_log->setVisible(visible);
        m_ui.logToggleBtn->setText(visible ? tr("Hide log") : tr("Show log"));
    });
}

void MainWindow::connectTools()
{
    SequenceEditorController* sequences = m_sequences.get();
    connect(m_ui.runSeqBtn, &StyledButton::clicked, this, [this]() { chooseAndRun(false); });
    connect(m_ui.runTemplateBtn, &StyledButton::clicked, this, [this]() { chooseAndRun(true); });
    connect(m_ui.editSeqBtn, &StyledButton::clicked, this, [sequences]() { sequences->editSequences(); });
    connect(m_ui.newSeqBtn, &StyledButton::clicked, this, [sequences]() { sequences->createNewSequence(); });
    connect(m_ui.editTemplatesBtn, &StyledButton::clicked, this, [sequences]() { sequences->showTemplates(); });
    connect(m_ui.exportSequenceAsTemplateBtn, &StyledButton::clicked, this,
            [sequences]() { sequences->exportSequenceAsTemplate(); });
    connect(m_ui.seqGraphBtn, &StyledButton::clicked, this, [sequences]() { sequences->refreshGraph(); });
    connect(m_ui.m_seqGraphWindow, &SequenceGraphWidget::sequenceSelected, this,
            [sequences](const QString& name) { sequences->openEditorFor(name); });
    connect(m_ui.graphBtn, &StyledButton::clicked, this, [this]() {
        m_ui.m_graphWindow->show();
        m_ui.m_graphWindow->raise();
        m_ui.m_graphWindow->activateWindow();
    });
    connect(m_ui.lightRagBtn, &StyledButton::clicked, this, &MainWindow::openLightRag);
    connect(m_ui.documentsBtn, &StyledButton::clicked, this, &MainWindow::openDocuments);
}

void MainWindow::restoreLayout()
{
    // First run: sizes that show every panel; later runs: the sizes the user left.
    if (m_ui.m_centerSplitter) {
        m_ui.m_centerSplitter->loadState();
        if (!m_ui.m_centerSplitter->hasSavedState())
            m_ui.m_centerSplitter->setSizes({430, 120, 230, 190});
    }
    m_ui.mainSplitter->loadState();
    if (!m_ui.mainSplitter->hasSavedState())
        m_ui.mainSplitter->setSizes({280, 780, 380});
    QList<int> sizes = m_ui.mainSplitter->sizes();
    if (sizes.size() >= 3 && sizes.at(2) == 0) {  // a collapsed right panel would hide the graph
        sizes[0] = qMax(200, sizes.at(0));
        sizes[1] = qMax(400, sizes.at(1));
        sizes[2] = 250;
        m_ui.mainSplitter->setSizes(sizes);
    }
}

// ---------------------------------------------------------------- requests and runs

void MainWindow::handleRequest(const ChatPanel::Request& request)
{
    Conversation& conversation = m_session.conversation();
    WorkingContext& context = m_session.context();
    const bool chat = request.mode == ChatPanel::Request::Mode::Chat;
    const QString text = chat ? request.text : request.textWithImages;

    if (request.images.isEmpty()) {
        conversation.clearImages();
    } else {
        conversation.setImages(request.images);
        context.addImages(request.images.paths, request.images.summaries);
    }
    context.observeRequest(text);
    showQuestion(request.textWithImages);

    switch (request.mode) {
    case ChatPanel::Request::Mode::Chat:
        m_ui.m_chat->appendUserMessage(request.images.isEmpty()
                ? request.text
                : request.text + QLatin1Char('\n') + tr("[Images attached: %n]", nullptr, request.images.paths.size()));
        m_engine.chat(text);
        break;
    case ChatPanel::Request::Mode::Template:
        conversation.addUserMessage(text);
        runTemplate(request.templatePath);
        break;
    case ChatPanel::Request::Mode::PickTemplate:
        conversation.addUserMessage(text);
        chooseAndRun(false);
        break;
    }
}

void MainWindow::runTemplate(const QString& templatePath)
{
    QString error;
    const WorkflowLibrary::Found found = m_session.workflows().mainSequenceOf(templatePath, &error);
    if (!found.isValid()) {
        QMessageBox::warning(this, tr("Error"), error);
        return;
    }
    m_sequences->setSelectedTemplate(templatePath);
    m_engine.start(found, templatePath);
}

void MainWindow::chooseAndRun(bool templatesOnly)
{
    WorkflowLibrary& workflows = m_session.workflows();
    workflows.refresh();
    RunTemplateOrSequenceDialog dialog(workflows.sequences().sequences(), &workflows.templates(), this);
    if (templatesOnly)
        dialog.setTemplateModeOnly();
    if (dialog.exec() != QDialog::Accepted)
        return;

    const bool sequenceMode = dialog.selectedMode() == RunTemplateOrSequenceDialog::RunSequence;
    if (templatesOnly && sequenceMode) {
        QMessageBox::warning(this, tr("Error"), tr("Please choose a template."));
        return;
    }
    const QString path = sequenceMode ? dialog.selectedSequencePath() : dialog.selectedDefaultSequencePath();
    const WorkflowLibrary::Found found = WorkflowLibrary::load(path);
    if (!found.isValid()) {
        QMessageBox::warning(this, tr("Error"), tr("Could not load the sequence."));
        return;
    }
    const QString templatePath = sequenceMode ? QString() : dialog.selectedTemplatePath();
    if (!templatePath.isEmpty())
        m_sequences->setSelectedTemplate(templatePath);
    m_engine.start(found, templatePath);
}

void MainWindow::showQuestion(const QString& text)
{
    if (text.trimmed().isEmpty())
        return;
    m_lastQuestionAt = QDateTime::currentDateTime();
    m_ui.m_resultDisplayWidget->appendResult(tr("User question"), QStringLiteral("\n\n%1\n").arg(text.trimmed()),
                                             ResultDisplayWidget::EntryType::UserQuestion);
}

void MainWindow::showResult(const QString& title, const QString& content, AgentEngine::ResultKind kind)
{
    const auto type = kind == AgentEngine::ResultKind::Reasoning ? ResultDisplayWidget::EntryType::Reasoning
                                                                  : ResultDisplayWidget::EntryType::Default;
    m_ui.m_resultDisplayWidget->appendResult(title, content, type,
                                             m_session.context().snapshotSince(m_lastQuestionAt));
}

// ---------------------------------------------------------------- project

void MainWindow::openProject(const QString& path)
{
    if (!path.trimmed().isEmpty())
        m_session.project().open(path);
}

void MainWindow::openFileInEditor(const QString& path)
{
    m_ui.ide_widget->openFile(QDir(m_session.project().root()).absoluteFilePath(path));
}

void MainWindow::browse()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Select project folder"), m_session.project().root());
    if (!folder.isEmpty())
        openProject(folder);
}

void MainWindow::showProjectFiles()
{
    Project& project = m_session.project();
    const QSet<QString> needDescription = project.filesNeedingDescription();
    m_ui.m_files->clear();
    for (const QString& rel : project.files()) {
        const QString abs = QDir(project.root()).filePath(rel);
        auto* item = new QListWidgetItem(rel, m_ui.m_files);
        // Red: no description yet, or out of date. Green: up to date.
        item->setBackground(needDescription.contains(abs) ? QColor(0xFD, 0xE8, 0xE8) : QColor(0xE3, 0xF6, 0xEA));
        item->setData(Qt::UserRole, abs);
    }
    m_ui.ide_widget->updateMissingFiles(needDescription);
    log(QStringLiteral("Rescanned project."));
}

void MainWindow::showContext()
{
    const ContextManager& history = m_session.context().commands();
    ContextTreeView::populate(m_ui.m_contextCommandsList, history);
    if (m_ui.m_contextGraphWidget)
        m_ui.m_contextGraphWidget->setGraphData(history.getContextGraphNodes(), history.activeContextGraphNodeId());
}

void MainWindow::undoLastChange()
{
    QString error;
    if (!m_session.project().manager().undoLastChange(&error)) {
        QMessageBox::information(this, tr("Undo"),
                                 error.isEmpty() || error == QLatin1String("Nothing to undo") ? tr("There is nothing to undo.") : error);
        return;
    }
    log(QStringLiteral("Undo done."));
    statusBar()->showMessage(tr("The last change was undone."), 5000);
    m_session.project().rescan();
}

void MainWindow::describeFiles()
{
    if (m_describer.isRunning())
        return;
    const QSet<QString> pending = m_session.project().filesNeedingDescription();
    if (pending.isEmpty()) {
        QMessageBox::information(this, tr("Done"), tr("No files in the project need a description."));
        return;
    }
    QStringList files = pending.values();
    files.sort();

    // Not modal: the model writes the descriptions in the background.
    auto* progress = new QProgressDialog(tr("Generating descriptions…"), tr("Cancel"), 0, files.size(), this);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setMinimumDuration(0);
    progress->setValue(0);
    connect(progress, &QProgressDialog::canceled, &m_describer, &DescriptionGenerator::cancel);
    connect(&m_describer, &DescriptionGenerator::progress, progress,
            [this, progress](int done, int, const QString& file, bool described) {
        progress->setValue(done);
        if (described)
            log(QStringLiteral("✓ Described %1").arg(QDir(m_session.project().root()).relativeFilePath(file)));
    });
    connect(&m_describer, &DescriptionGenerator::finished, progress, [this, progress](int described, int failed) {
        log(QStringLiteral("Description generation finished: %1 succeeded, %2 failed.").arg(described).arg(failed));
        progress->close();
        m_session.project().rescan();
    });
    m_describer.start(files);
}

// ---------------------------------------------------------------- settings and tools

void MainWindow::applyModelSettings()
{
    ModelSettingsPanel* panel = m_ui.m_llmSettingsWindow;
    LlmClient& model = m_session.model();
    const LlmClient::Provider provider = LlmClient::providerFromString(panel->provider());
    QUrl endpoint(panel->endpoint().trimmed());
    if (!endpoint.isValid() || endpoint.isEmpty())
        endpoint = LlmClient::defaultEndpoint(provider);

    AppConfig config;
    config.load();
    if (provider != model.provider())
        config.selectedModel.clear();  // the models of the other server are different
    config.provider = LlmClient::providerToString(provider);
    config.endpoint = endpoint;
    config.setGenerationParams(panel->params());
    config.save();

    m_session.applyModelSettings(config);
    m_ui.m_chat->refreshModels();
    showModelInStatusBar();
    log(QStringLiteral("LLM settings applied: provider=%1 endpoint=%2").arg(model.providerName(), endpoint.toString()));
}

void MainWindow::showModelInStatusBar()
{
    const QString name = m_session.model().model().trimmed();
    statusBar()->showMessage(tr("Provider: %1 | Model: %2")
                                     .arg(m_session.model().providerName(), name.isEmpty() ? tr("<not selected>") : name));
}

void MainWindow::openDocuments()
{
    if (!m_documents) {
        m_documents = new Rag::RagWidget();
        m_documents->setAttribute(Qt::WA_DeleteOnClose, true);
        m_documents->setFolder(m_session.project().root());
    }
    m_documents->show();
    m_documents->raise();
    m_documents->activateWindow();
}

void MainWindow::openLightRag()
{
    if (!m_lightRag) {
        m_lightRag = new LightRagWidget();
        m_lightRag->setAttribute(Qt::WA_DeleteOnClose, true);
    }
    m_lightRag->show();
    m_lightRag->raise();
    m_lightRag->activateWindow();
}

void MainWindow::log(const QString& line)
{
    m_ui.m_log->append(line);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_ui.m_seqGraphWindow)
        m_ui.m_seqGraphWindow->saveLayout();
    QMainWindow::closeEvent(event);
}
