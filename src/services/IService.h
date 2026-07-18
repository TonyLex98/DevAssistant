#pragma once
#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
#include <QByteArray>
#include <QTreeWidgetItem>
#include <QDateTime>
#include <QPair>

// Forward declarations
class QTreeWidget;
class QListWidget;
class QComboBox;
class QFileSystemWatcher;
class OllamaAdapter;
class DevAssManager;
class PromptSequenceManager;
class PromptSequence;
class PromptStep;
class Command;
class ChangeApplier;
class ContextManager;
class ContextCacheManager;
class SolutionTemplateManager;
class TaskStatusController;
class TtsController;
class SequenceGraphWidget;

// Included for Change type and ResultDisplayWidget
#include "../core/ChangeApplier.h"
#include "../ui/ResultDisplayWidget.h"

/*
 * Service interfaces for dependency injection into controllers.
 *
 * Controllers depend on these interfaces instead of MainWindow directly.
 * MainWindow implements all interfaces and injects them at construction.
 * Controllers communicate back to MainWindow via Q_SIGNALs.
 */

// ─── Context operations ────────────────────────────────────────────────
class IContextService {
public:
    virtual ~IContextService() = default;

    // ContextCache operations
    virtual void setFullFileContext(const QString& path, const QStringList& lines) = 0;
    virtual void addRegionContext(const QString& path, int startLine, const QStringList& lines) = 0;
    virtual void clearContextForPath(const QString& path) = 0;
    virtual void clearAll() = 0;
    virtual QString contextContent() const = 0;
    virtual void appendConsoleLog(const QString& log) = 0;
    virtual void removeConsoleLogsWithPrefix(const QString& prefix) = 0;
    virtual void appendExternalSource(const QString& source) = 0;
    virtual void clearConsoleLogs() = 0;
    virtual void clearExternalSources() = 0;
    virtual QStringList consoleLogs() const = 0;
    virtual QStringList externalSources() const = 0;
    virtual QString formatNumberedLines(const QStringList& lines, int startLine) const = 0;

    // ContextManager operations
    virtual bool hasContextManager() const = 0;
    virtual void observeUserQuery(const QString& query) = 0;
    virtual void addCommandToContext(int cmdIndex, const QString& description, const QString& result = QString()) = 0;
    virtual QString generateContextText() const = 0;
    virtual QString getAllCommandsText() const = 0;

    // ContextManager data access (for tree view population)
    struct SeqInfo {
        QString id;
        QString name;
        QString description;
        QDateTime timestamp;
    };
    struct CmdInfo {
        QString id;
        QString description;
        QString result;
        QString sequenceId;
        int state; // CommandState enum
        QString reason;
        QDateTime timestamp;
    };
    virtual QList<SeqInfo> getCommandSequences() const = 0;
    virtual SeqInfo getSequenceInfo(const QString& id) const = 0;
    virtual QList<CmdInfo> getAllCommands() const = 0;
    virtual QList<CmdInfo> getCommandsInSequence(const QString& seqId) const = 0;

    // File-to-context mapping
    virtual bool fileHasContextCommands(const QString& path) const = 0;
    virtual QSet<QString> getContextCommandsForFile(const QString& path) const = 0;
    virtual void clearFileToContextCommands() = 0;
    virtual void insertFileContextCommand(const QString& path, const QString& cmd) = 0;

    // Shell/Python output tracking
    virtual void clearShellCommandOutputs() = 0;
    virtual void clearPythonScriptOutputs() = 0;
    virtual void removeShellOutput(const QString& key) = 0;
    virtual void removePythonOutput(const QString& key) = 0;

    // File watcher
    virtual QFileSystemWatcher* fileWatcher() = 0;

    // Direct access for SequenceRunner
    virtual ContextManager* contextManagerPtr() = 0;
    virtual ContextCacheManager* contextCachePtr() = 0;
    virtual QMap<QString, QSet<QString>>& fileToContextCommands() = 0;
    virtual QMap<QString, QString>& shellCommandOutputs() = 0;
    virtual QMap<QString, QString>& pythonScriptOutputs() = 0;

    // Command handlers (delegated to MainWindow)
    virtual void handleOpenCommand(Command* cmd) = 0;
    virtual void handlePostApplyContextCleanup(Command* cmd) = 0;
    virtual void handleSearchResults(Command* cmd) = 0;
    virtual void addCommandToContext(Command* cmd, const QString& result = QString()) = 0;
    virtual void appendConsoleContext(const QString& title, const QString& stdOut, const QString& stdErr, int exitCode) = 0;
    virtual void appendExternalContext(const QString& block) = 0;
};

// ─── Model + chat operations ───────────────────────────────────────────
class IModelService {
public:
    virtual ~IModelService() = default;

    virtual void configure(const QString& model, const QVariantMap& params) = 0;
    virtual void setProvider(int provider) = 0;
    virtual void setEndpoint(const QUrl& url) = 0;
    virtual QString selectedModel() const = 0;
    virtual int provider() const = 0;
    virtual QString providerName() const = 0;

    // Chat dialog
    virtual void appendAssistantChunk(const QString& chunk) = 0;
    virtual void refreshModelList() = 0;
    virtual bool hasChat() const = 0;

    // Stream buffer
    virtual QString& streamBuf() = 0;

    // History
    virtual QList<QPair<QString, QString>>& history() = 0;
    virtual void appendHistory(const QString& role, const QString& content) = 0;
    virtual QList<QString>& lastUserMessages() = 0;
    virtual void appendLastUserMessage(const QString& msg) = 0;
    virtual int maxUserMessages() const = 0;

    // Last messages
    virtual QString& lastAssistantMessage() = 0;
    virtual QString& currentPrompt() = 0;

    // Images
    virtual QStringList& activeImagesBase64() = 0;

    // Model pointers
    virtual OllamaAdapter* modelPtr() = 0;
    virtual OllamaAdapter* agentModelPtr() = 0;

    // Last user message
    virtual void setLastUserMessage(const QString& msg) = 0;
    virtual QString lastUserMessage() const = 0;

    // Agent model
    virtual QString selectedAgentModel() const = 0;
    virtual void configureAgentModel(const QString& model, const QVariantMap& params) = 0;
    virtual void setAgentProvider(int provider) = 0;
    virtual void setAgentEndpoint(const QUrl& url) = 0;

    // JSON extraction utilities
    virtual QByteArray extractJsonPart(const QByteArray& raw) const = 0;
    virtual QString summarizeSearchResults(const QString& body, const QString& query) = 0;
};

// ─── Project + file operations ─────────────────────────────────────────
class IProjectService {
public:
    virtual ~IProjectService() = default;

    virtual QString projectRoot() const = 0;
    virtual void setProjectRoot(const QString& path) = 0;
    virtual QStringList indexedFiles() const = 0;
    virtual void setIndexRoot(const QString& path) = 0;

    // DevAssistant manager
    virtual void loadDevManager() = 0;
    virtual QString devAssDirPath() const = 0;
    virtual struct FileDescription* findFileDescription(const QString& path) = 0;
    virtual void setDevProjectRoot(const QString& path) = 0;
    virtual bool createFolder() = 0;
    virtual bool createDaproFile() = 0;
    virtual bool generateDescrFiles() = 0;

    // Prompt manager
    virtual void setPromptProjectRoot(const QString& path) = 0;
    virtual void* promptMgr() = 0; // for PromptEditorDialog

    // Files list UI
    virtual QListWidget* filesList() = 0;

    // IDE widget
    virtual void openProjectInIDE(const QString& path) = 0;

    // Root edit
    virtual void setRootEditText(const QString& path) = 0;

    // Signal
    virtual void emitMissingFilesUpdated(const QSet<QString>& paths) = 0;
    virtual void rescan() = 0;
    virtual DevAssManager* devManagerPtr() = 0;

    // Edit in range
    virtual bool applyGroupedEditInRange(const QList<Command*>& commands, QString* err) = 0;
};

// ─── UI update operations ──────────────────────────────────────────────
class IUIUpdater {
public:
    virtual ~IUIUpdater() = default;

    virtual void log(const QString& msg) = 0;
    virtual void statusBarMessage(const QString& msg) = 0;
    virtual void appendUserQuestionToResultDisplay(const QString& question) = 0;
    virtual void appendConsoleToResultDisplay(const QString& block) = 0;
    virtual void appendExternalToResultDisplay(const QString& block) = 0;

    // Context display
    virtual QTreeWidget* contextCommandsList() = 0;

    // Result display
    virtual ResultDisplayWidget* resultDisplayWidget() = 0;
    virtual QDateTime& lastQuestionTimestamp() = 0;

    // Confirm mode
    virtual QComboBox* confirmModeComboBox() = 0;
};

// ─── Sequence execution operations ─────────────────────────────────────
class ISequenceService {
public:
    virtual ~ISequenceService() = default;

    virtual bool isStopRequested() const = 0;
    virtual void setStopRequested(bool v) = 0;
    virtual bool isSequenceMode() const = 0;
    virtual void setSequenceMode(bool v) = 0;
    virtual void resetSequenceStabilityState() = 0;
    virtual void runSequenceDialog() = 0;
    virtual void stopTask() = 0;
    virtual PromptSequenceManager* seqMgrPtr() = 0;
    virtual PromptSequence& currentSeq() = 0;
    virtual PromptSequence* currentSequencePtr() = 0;
    virtual QStringList& prevSteps() = 0;
    virtual int currentStepIndex() const = 0;
    virtual void setCurrentStepIndex(int idx) = 0;
    virtual QString currentSeqPath() const = 0;
    virtual void setCurrentSeqPath(const QString& path) = 0;
    virtual QString currentSeqId() const = 0;
    virtual void setCurrentSeqId(const QString& id) = 0;
    virtual QString currentTemplatePath() const = 0;
    virtual void setCurrentTemplatePath(const QString& path) = 0;
    virtual bool useAgentPipeline() const = 0;
    virtual void setUseAgentPipeline(bool v) = 0;
    virtual int& agentRepairCountThisStep() = 0;
    virtual int agentRepairCount() const = 0;
    virtual void setAgentRepairCount(int v) = 0;
    virtual int agentMaxRepairs() const = 0;
    virtual bool inAgentAutoRepair() const = 0;
    virtual void setInAgentAutoRepair(bool v) = 0;
    virtual int agentRepairStepIdx() const = 0;
    virtual void setAgentRepairStepIdx(int v) = 0;
    virtual QString agentRepairSeqId() const = 0;
    virtual void setAgentRepairSeqId(const QString& id) = 0;
    virtual QList<Change>& pendingChanges() = 0;
    virtual ChangeApplier* applierPtr() = 0;
    virtual ResultDisplayWidget::ResultEntryMetadata collectContextMetadataSince(const QDateTime& since) const = 0;

    // Sequence management
    virtual void refreshAllSequences() = 0;
    virtual void ensureDefaultSequence() = 0;
    virtual void updateSequenceGraphHighlight() = 0;
};

// ─── Graph window operations ───────────────────────────────────────────
class IGraphService {
public:
    virtual ~IGraphService() = default;

    virtual void showGraphWindow() = 0;
    virtual void showLightRagWindow() = 0;
    virtual SequenceGraphWidget* sequenceGraphWindow() = 0;
};

// ─── LLM settings operations ───────────────────────────────────────────
class ISettingsService {
public:
    virtual ~ISettingsService() = default;

    virtual void showSettings() = 0;
    virtual void hideSettings() = 0;
    virtual bool isSettingsVisible() const = 0;
    virtual void reloadSettingsFromConfig() = 0;
    virtual void setSettingsButtonText(const QString& text) = 0;
    virtual QString providerString() const = 0;
    virtual QString endpointString() const = 0;
    virtual QVariantMap paramsMap() const = 0;
};

// ─── Template management operations ─────────────────────────────────────
class ITemplateService {
public:
    virtual ~ITemplateService() = default;

    virtual SolutionTemplateManager* templateMgrPtr() = 0;
};

// ─── Status controller operations ──────────────────────────────────────
class IStatusControllerService {
public:
    virtual ~IStatusControllerService() = default;

    virtual TaskStatusController* statusControllerPtr() = 0;
};

// ─── TTS controller operations ─────────────────────────────────────────
class ITTSService {
public:
    virtual ~ITTSService() = default;

    virtual TtsController* ttsControllerPtr() = 0;
};
