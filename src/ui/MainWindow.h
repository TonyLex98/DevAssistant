#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <StyleTextEdit.h>
#include <QPair>
#include <QFileSystemWatcher>
#include <runSequenceDialog.h>
#include "../core/ProjectIndex.h"
#include "../core/ChangeApplier.h"
#include "../adapters/OllamaAdapter.h"
#include "../utils/PromptManager.h"
#include <StyledButton.h>
#include <DevAssManager.h>
#include <QProgressDialog>
#include "DescriptionDialog.h"      // ← новый диалог
#include <CommandParser.h>
#include <CommandConfirmDialog.h>
#include <OpenFileContextCommand.h>
#include <QString>
#include "../audio/VoskWidget.h"
#include <PromptSequenceManager.h>
#include <QMessageBox>
#include  <PromptResolver.h>
#include <QDateTime>
#include <QTimer>
#include <PromptSequenceEditor.h>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <IDEWidget.h>
#include "../ui/ResultDisplayWidget.h"
#include <QToolBar>
#include <QDockWidget>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QStandardPaths>
#include <QTabWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QFrame>
#include <MemoryLineEdit.h>
#include <ProjectRelationView.h>
#include "GraphWidget.h"
#include "ContextGraphWidget.h"
#include <QMap>
#include <QVector>
#include "customWidgets/MemoryComboBox.h"
#include "../core/ContextManager.h"
#include "LightRagWidget.h"

// Need the complete definition for sequence graph widget
#include "../codeviewer/SequenceGraphWidget.h"

// Solution templates
#include "../core/SolutionTemplateManager.h"

// TTS
#include "../audio/TtsRunner.h"

// Controllers
#include "controllers/TtsController.h"
#include "controllers/TaskStatusController.h"
#include "controllers/ProjectBrowserController.h"
#include "controllers/SettingsPageBuilder.h"
#include "controllers/ContextCacheManager.h"
#include "controllers/SequenceRunner.h"
#include "controllers/SequenceEditorController.h"
#include "controllers/GraphController.h"
#include "controllers/DescriptionController.h"
#include "controllers/ModelConfigController.h"
#include "controllers/ContextDisplayController.h"
#include "controllers/ChatController.h"
#include "controllers/UiBuilder.h"
#include "../services/IService.h"

class ChatDialog;
class ModelsDialog;
class OllamaSettingsDialog;
class PromptEditorDialog;
class ChangesDialog;

class SequenceRunner;
class SequenceEditorController;

class MainWindow : public QMainWindow,
                   public IContextService,
                   public IModelService,
                   public IProjectService,
                   public IUIUpdater,
                   public ISequenceService,
                   public IGraphService,
                   public ISettingsService,
                   public ITemplateService,
                   public IStatusControllerService,
                   public ITTSService {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);

    const QString& projectRootPath() const { return m_projectRoot; }
    PromptManager*  promptManager() { return &m_promptMgr; }
    const QString& openedFilesContent() const { return m_contextContent; }

    QString searchResultsText() const;
    QString getContextChainText() const; // Public access to context chain (only active commands)

    void updateChatToggleButton(bool visible);

    // ─── IContextService ──────────────────────────────────────────────
    void setFullFileContext(const QString& path, const QStringList& lines) override;
    void addRegionContext(const QString& path, int startLine, const QStringList& lines) override;
    void clearContextForPath(const QString& path) override;
    void clearAll() override;
    QString contextContent() const override;
    void appendConsoleLog(const QString& log) override;
    void removeConsoleLogsWithPrefix(const QString& prefix) override;
    void appendExternalSource(const QString& source) override;
    void clearConsoleLogs() override;
    void clearExternalSources() override;
    QStringList consoleLogs() const override;
    QStringList externalSources() const override;
    QString formatNumberedLines(const QStringList& lines, int startLine) const override;
    bool hasContextManager() const override;
    void observeUserQuery(const QString& query) override;
    void addCommandToContext(int cmdIndex, const QString& description, const QString& result = QString()) override;
    QString generateContextText() const override;
    QString getAllCommandsText() const override;
    QList<IContextService::SeqInfo> getCommandSequences() const override;
    IContextService::SeqInfo getSequenceInfo(const QString& id) const override;
    QList<IContextService::CmdInfo> getAllCommands() const override;
    QList<IContextService::CmdInfo> getCommandsInSequence(const QString& seqId) const override;
    bool fileHasContextCommands(const QString& path) const override;
    QSet<QString> getContextCommandsForFile(const QString& path) const override;
    void clearFileToContextCommands() override;
    void insertFileContextCommand(const QString& path, const QString& cmd) override;
    void clearShellCommandOutputs() override;
    void clearPythonScriptOutputs() override;
    void removeShellOutput(const QString& key) override;
    void removePythonOutput(const QString& key) override;
    QFileSystemWatcher* fileWatcher() override;

    // IContextService - additional accessors
    ContextManager* contextManagerPtr() override;
    ContextCacheManager* contextCachePtr() override;
    QMap<QString, QSet<QString>>& fileToContextCommands() override;
    QMap<QString, QString>& shellCommandOutputs() override;
    QMap<QString, QString>& pythonScriptOutputs() override;
    void handleOpenCommand(Command* cmd) override;
    void handlePostApplyContextCleanup(Command* cmd) override;
    void handleSearchResults(Command* cmd) override;
    void addCommandToContext(Command* cmd, const QString& result = QString()) override;
    void appendConsoleContext(const QString& title, const QString& stdOut, const QString& stdErr, int exitCode) override;
    void appendExternalContext(const QString& block) override;

    // ─── IModelService ────────────────────────────────────────────────
    void configure(const QString& model, const QVariantMap& params) override;
    void setProvider(int provider) override;
    void setEndpoint(const QUrl& url) override;
    QString selectedModel() const override;
    int provider() const override;
    QString providerName() const override;
    void appendAssistantChunk(const QString& chunk) override;
    void refreshModelList() override;
    bool hasChat() const override;
    QString& streamBuf() override;
    void appendHistory(const QString& role, const QString& content) override;
    void appendLastUserMessage(const QString& msg) override;
    int maxUserMessages() const override;
    OllamaAdapter* modelPtr() override;
    void setLastUserMessage(const QString& msg) override;
    QString lastUserMessage() const override;
    QString selectedAgentModel() const override;
    void configureAgentModel(const QString& model, const QVariantMap& params) override;
    void setAgentProvider(int provider) override;
    void setAgentEndpoint(const QUrl& url) override;

    // IModelService - additional accessors
    QList<QPair<QString, QString>>& history() override;
    QList<QString>& lastUserMessages() override;
    QString getLastUserMessages() const; // formatted string for PromptResolver
    QString& lastAssistantMessage() override;
    QString& currentPrompt() override;
    QStringList& activeImagesBase64() override;
    OllamaAdapter* agentModelPtr() override;
    QByteArray extractJsonPart(const QByteArray& txt) const override;
    QString summarizeSearchResults(const QString& content, const QString& query) override;

    // ─── IProjectService ──────────────────────────────────────────────
    QString projectRoot() const override;
    void setProjectRoot(const QString& path) override;
    QStringList indexedFiles() const override;
    void setIndexRoot(const QString& path) override;
    void loadDevManager() override;
    QString devAssDirPath() const override;
    FileDescription* findFileDescription(const QString& path) override;
    void setDevProjectRoot(const QString& path) override;
    bool createFolder() override;
    bool createDaproFile() override;
    bool generateDescrFiles() override;
    void setPromptProjectRoot(const QString& path) override;
    void* promptMgr() override;
    QListWidget* filesList() override;
    void openProjectInIDE(const QString& path) override;
    void setRootEditText(const QString& path) override;
    void emitMissingFilesUpdated(const QSet<QString>& paths) override;
    void rescan() override;
    DevAssManager* devManagerPtr() override;

    // IProjectService - additional
    bool applyGroupedEditInRange(const QList<Command*>& commands, QString* err) override;

    // ─── IUIUpdater ───────────────────────────────────────────────────
    void log(const QString& msg) override;
    void statusBarMessage(const QString& msg) override;
    void appendUserQuestionToResultDisplay(const QString& question) override { if (m_sequenceRunner) m_sequenceRunner->appendUserQuestionToResultDisplay(question); }
    void appendConsoleToResultDisplay(const QString& block) override;
    void appendExternalToResultDisplay(const QString& block) override;
    QTreeWidget* contextCommandsList() override;

    // IUIUpdater - additional
    ResultDisplayWidget* resultDisplayWidget() override;
    QDateTime& lastQuestionTimestamp() override;
    QComboBox* confirmModeComboBox() override;

    // ─── ISequenceService ─────────────────────────────────────────────
    bool isStopRequested() const override;
    void setStopRequested(bool v) override;
    bool isSequenceMode() const override;
    void resetSequenceStabilityState() override { if (m_sequenceRunner) m_sequenceRunner->resetSequenceStabilityState(); }
    void runSequenceDialog() override { if (m_sequenceRunner) m_sequenceRunner->runSequenceDialog(); }
    void stopTask() override;
    PromptSequenceManager* seqMgrPtr() override;
    // Sequence editing and execution now live in SequenceEditorController and SequenceRunner.
    void refreshAllSequences() override { if (m_sequenceEditor) m_sequenceEditor->refreshAllSequences(); }
    void ensureDefaultSequence() override { if (m_sequenceEditor) m_sequenceEditor->ensureDefaultSequence(); }
    void updateSequenceGraphHighlight() override { if (m_sequenceEditor) m_sequenceEditor->updateSequenceGraphHighlight(); }
    void stopTemplateExecution() { if (m_sequenceRunner) m_sequenceRunner->stopExecution(); }
    PromptSequence* currentSequencePtr() override;
    int currentStepIndex() const override;
    void setCurrentStepIndex(int idx) override;
    QString currentSeqPath() const override;
    void setCurrentSeqPath(const QString& path) override;
    QString currentSeqId() const override;
    void setCurrentSeqId(const QString& id) override;
    QString currentTemplatePath() const override;
    void setCurrentTemplatePath(const QString& path) override;
    bool useAgentPipeline() const override;
    void setUseAgentPipeline(bool v) override;
    int agentRepairCount() const override;
    void setAgentRepairCount(int v) override;
    int agentMaxRepairs() const override;
    bool inAgentAutoRepair() const override;
    void setInAgentAutoRepair(bool v) override;
    int agentRepairStepIdx() const override;
    void setAgentRepairStepIdx(int v) override;
    QString agentRepairSeqId() const override;
    void setAgentRepairSeqId(const QString& id) override;
    QList<Change>& pendingChanges() override;
    ChangeApplier* applierPtr() override;
    ResultDisplayWidget::ResultEntryMetadata collectContextMetadataSince(const QDateTime& since) const override;

    // ISequenceService - additional
    PromptSequence& currentSeq() override;
    QStringList& prevSteps() override;
    void setSequenceMode(bool v) override;
    int& agentRepairCountThisStep() override;

    // ─── IGraphService ────────────────────────────────────────────────
    void showGraphWindow() override;
    void showLightRagWindow() override;
    SequenceGraphWidget* sequenceGraphWindow() override;

    // ─── ISettingsService ─────────────────────────────────────────────
    void showSettings() override;
    void hideSettings() override;
    bool isSettingsVisible() const override;
    void reloadSettingsFromConfig() override;
    void setSettingsButtonText(const QString& text) override;
    QString providerString() const override;
    QString endpointString() const override;
    QVariantMap paramsMap() const override;

    // ITemplateService
    SolutionTemplateManager* templateMgrPtr() override;
    // IStatusControllerService
    TaskStatusController* statusControllerPtr() override;
    // ITTSService
    TtsController* ttsControllerPtr() override;

public slots:
    void slot_updatelastUserMessage(const QString& lastUserMessage);

protected:
    void closeEvent(QCloseEvent *event) override;

signals:

    void missingFilesUpdated(const QSet<QString> &paths);


private slots:
    void browse();
    void openSettings();
    void editBasePrompt();
    void applyChanges();
    void undoLast();

    void generateDescriptions();      // <‑‑ новый слот
    void onVoskText(const QString&);  // <‑‑ новый слот
    void onOmniAsrText(const QString&);

    void openGraph();
    // New slots for template selection combobox in ChatDialog
    void runTemplateRequested(const QString &templatePath, const QString &text);
    void runSimpleChat(const QString &text);
    void runTemplateRequestedWithImages(const QString &templatePath,
                                        const QString &text,
                                        const QStringList &imagePaths,
                                        const QStringList &imagesBase64,
                                        const QStringList &imageSummaries);
    void runSimpleChatWithImages(const QString &text,
                                 const QStringList &imagePaths,
                                 const QStringList &imagesBase64,
                                 const QStringList &imageSummaries);
    void sendUserMessageWithImages(const QString &text,
                                   const QStringList &imagePaths,
                                   const QStringList &imagesBase64,
                                   const QStringList &imageSummaries);
    void openLightRag();


public:
    // Accessors used by PromptResolver
    QString previousStepsText() const;
    QString fileDescriptionsText() const;
    PromptSequenceManager& sequenceManager() { return m_seqMgr; }  // Access to sequence manager
    QString currentSequencePath() const { return m_currentSeqPath; }  // Access to current sequence path
    ProjectIndex& projectIndex() { return m_index; }  // Access to project index for structure resolver

private slots:    //void onFileDoubleClicked(QListWidgetItem *item);
    void setupModelWithGenerationParams(); // Configure model with generation parameters
    void onFileChanged(const QString &path); // Handle external file changes
private:
    QString buildProjectContext() const;

    void sendUserMessage(const QString& text);
    void onModelChunk(const QString& s);

private:
    QString m_projectRoot;
    ProjectIndex m_index;
    ChangeApplier m_applier;
    OllamaAdapter m_model;
    OllamaAdapter m_agentModel;
    PromptManager m_promptMgr;

    // Task status controller (timer + status bar)
    TaskStatusController* m_statusController{nullptr};
    QString m_currentPrompt;  // Current prompt being sent to the model (needed for context estimation)
    
    // File system monitoring to track external file changes
    QFileSystemWatcher* m_fileWatcher;
    QMap<QString, QSet<QString>> m_fileToContextCommands; // Maps files to commands that created context from them
    QMap<QString, QString> m_shellCommandOutputs; // Maps shell commands to their output titles
    QMap<QString, QString> m_pythonScriptOutputs; // Maps python scripts to their output titles
    
    // New context management system
    ContextManager* m_contextManager;
    QWidget* m_contextStatusWidget; // Widget to display context command status
    QTabWidget* m_contextTabs = nullptr; // Tabs: command list / context graph
    QTreeWidget* m_contextCommandsList; // Tree to show which commands are in context with expandable details
    ContextGraphWidget* m_contextGraphWidget = nullptr;

    QListWidget* m_files=nullptr;
    MemoryLineEdit* m_rootEdit=nullptr;
    StyleTextEdit* m_log=nullptr;

    ChatDialog* m_chat=nullptr;
    QList<Change> m_pending;

    QList<QPair<QString,QString>> m_history;
    QString m_streamBuf;
    QStringList m_activeImagesBase64;
    QStringList m_activeImagePaths;
    QStringList m_activeImageSummaries;
    bool m_preserveActiveImagesForNextRun = false;

    bool m_useAgentPipeline = true;
    int m_agentRepairCountThisStep = 0;
    int m_agentMaxRepairsPerStep = 2;
    bool m_inAgentAutoRepair = false;
    int m_agentRepairStepIdx = -1;
    QString m_agentRepairSeqId;

    StyledButton* rescanBtn;
    StyledButton* settingsBtn;
    OllamaSettingsDialog* m_llmSettingsWindow = nullptr;
    StyledButton* promptBtn;
    StyledButton* applyBtn;
    StyledButton* undoBtn;
    StyledButton* browseBtn;
    StyledButton* runSeqBtn;
    StyledButton *editSeqBtn;
    StyledButton *newSeqBtn;
    StyledButton *runTemplateBtn;
    StyledButton *editTemplatesBtn;
    StyledButton *exportSequenceAsTemplateBtn;
    MemoryComboBox* confirmModeCombo;
    DevAssManager m_dev_manager;
    StyledButton* descBtn = nullptr;   // <‑‑ новая кнопка
    VoskWidget* m_voskWidget = nullptr;  // <‑‑ виджет Vosk

    QString m_contextContent;

    QStringList m_prevSteps;

    // ... уже есть
    PromptSequenceManager m_seqMgr;          // менеджер наборов
    SolutionTemplateManager m_templateMgr;   // менеджер шаблонов
    PromptSequence        m_currentSeq;     // текущая выбранная последовательность
    bool                  m_sequenceMode = false;
    bool                  m_stopRequested = false; // Flag to indicate that the sequence execution should be stopped
    int                   m_currentStepIdx = -1; // -1 = не запущено  // Run the next sequence specified by OPEN_SEQUENCE
    void addSequenceTransitionToHistory(const QString& sequenceName);  // Add sequence transition to history
    
    /* ────── NEW ────── */    void ensureUniversalAgentTemplateInternal(); // Write UniversalAgent template v3 if missing/outdated
    /* ────── END ────── */

    void reOpenRootDir();

    QString m_lastUserMessage;
    QString m_lastAssistantMessage;
    QDateTime m_lastQuestionTimestamp;
    QString m_currentSeqPath; // Path of the currently running sequence (for finding related sequences)
    QString m_currentSeqId;   // ID of the current command sequence being tracked
    QString m_currentTemplatePath;
    IDEWidget *ide_widget;
    
    // Separate storage for last user messages (for LAST_USER_MESSAGES resolver)
    QList<QString> m_lastUserMessages;
    static const int MAX_USER_MESSAGES = 100; // Maximum number of user messages to store

    MemorySplitter *mainSplitter;
    MemorySplitter *m_centerSplitter{nullptr};

    QTabWidget *project_view_tabs = new QTabWidget();
    ProjectRelationView *project_view = new ProjectRelationView;

    StyledButton *graphBtn{nullptr};    // кнопка «Show Graph»
    StyledButton *seqGraphBtn{nullptr}; // кнопка «Show Sequence Graph»
    StyledButton *chatToggleBtn{nullptr};
    StyledButton *logToggleBtn{nullptr};
    StyledButton *lightRagBtn{nullptr}; // кнопка «LightRAG»
    QWidget *m_settingsPanel{nullptr};
    GraphWidget  *m_graphWindow{nullptr};   // окно-виджет графа
    SequenceGraphWidget  *m_seqGraphWindow{nullptr};   // окно-виджет графа последовательностей
    LightRagWidget *m_lightRagWidget{nullptr}; // LightRAG widget
    TtsController* m_ttsController{nullptr}; // TTS controller (toggle + runner)
    SettingsPageBuilder m_settingsBuilder; // Builds 6 settings pages
    ContextCacheManager m_contextCache; // Manages context slices cache
    SequenceRunner* m_sequenceRunner{nullptr}; // Sequence execution orchestrator
    SequenceEditorController* m_sequenceEditor{nullptr}; // Sequence editor controller
    GraphController* m_graphController{nullptr};
    DescriptionController* m_descriptionController{nullptr};
    ModelConfigController* m_modelConfigController{nullptr};
    ContextDisplayController* m_contextDisplayController{nullptr};
    ChatController* m_chatController{nullptr};
    ProjectBrowserController* m_projectBrowserController{nullptr};

    ResultDisplayWidget* m_resultDisplayWidget = nullptr;
    QStringList buildAllowedCommandsDocsCompact(const PromptStep& step) const;
    QByteArray repairToJsonArrayIfNeeded(const QByteArray& rawText, const PromptStep& step);
    QString commandBatchSignature(const QList<Command*>& commands) const;
    bool isReadOnlyCommandBatch(const QList<Command*>& commands) const;
    void appendResultToUi(const QString& title, const QString& content);
    void autoFinalizeSequenceWithResult(const QString& reason, const QList<Command*>& recentCommands);
    bool maybeAutoRepairAfterExecution(const PromptStep& step,
                                       const QDateTime& execStartedAt,
                                       const QList<Command*>& executedCommands,
                                       bool applyFailed,
                                       const QString& lastError);
    static bool isImplementationIntentText(const QString& text);
    bool didAnyAffectedPathChangeSince(const QDateTime& since, const QList<Command*>& executedCommands) const;

private:
    void updateContextStatusDisplay(); // Update the visual display of context commands

};
