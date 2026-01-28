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
#include <QMap>
#include <QVector>
#include "customWidgets/MemoryComboBox.h"
#include "../core/ContextManager.h"
#include "LightRagWidget.h"

// Need the complete definition for sequence graph widget
#include "../codeviwer/SequenceGraphWidget.h"

// Solution templates
#include "../core/SolutionTemplateManager.h"

// TTS
#include "../audio/TtsRunner.h"

class ChatDialog;
class ModelsDialog;
class OllamaSettingsDialog;
class PromptEditorDialog;
class ChangesDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);

    const QString& projectRootPath() const { return m_projectRoot; }
    PromptManager*  promptManager() { return &m_promptMgr; }
    const QList<QPair<QString,QString>>& history() const { return m_history; }
    const QString& openedFilesContent() const { return m_contextContent; }
    const QString& lastUserMessage() const { return m_lastUserMessage; }
    const QString& lastAssistantMessage() const { return m_lastAssistantMessage; }

    QString searchResultsText() const;
    QString getContextChainText() const; // Public access to context chain (only active commands)
    QString getAllCommandsText() const;  // Public access to all commands (for full chain view)
    
    // Accessor for last user messages (for resolver)
    QString getLastUserMessages() const;

    void updateChatToggleButton(bool visible);

public slots:
    void runSequenceDialog();
    void slot_updatelastUserMessage(const QString& lastUserMessage);
    void stopTemplateExecution();

protected:
    void closeEvent(QCloseEvent *event) override;

signals:

    void missingFilesUpdated(const QSet<QString> &paths);


private slots:
    void browse();
    void rescan();
    void openSettings();
    void editBasePrompt();
    void applyChanges();
    void undoLast();

    void handleAssistantFinished();

    void generateDescriptions();      // <‑‑ новый слот
    void onVoskText(const QString&);  // <‑‑ новый слот
    void onOmniAsrText(const QString&);

    void openGraph();
    void openSequenceGraph();
    void openSequenceEditorForName(const QString &sequenceName);
    void updateSequenceGraphHighlight();  // Update the highlighted sequence in the graph

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

private slots:
    void createNewSequence();
    //void onFileDoubleClicked(QListWidgetItem *item);
    void setupModelWithGenerationParams(); // Configure model with generation parameters
    void onFileChanged(const QString &path); // Handle external file changes
    void onChatTemplateSelectionChanged(const QString &templatePath);

private:
    QString buildProjectContext() const;
    void log(const QString& m);
    void openSequenceEditorWindow(const QString& title, const QString& filePath, const PromptSequence& seq);

    void sendUserMessage(const QString& text);
    void onModelChunk(const QString& s);
    QByteArray extractJsonPart(const QByteArray& txt) const;
    void editSequences();
    void updateSequenceGraphTemplateSelection();


private:
    QString m_projectRoot;
    ProjectIndex m_index;
    ChangeApplier m_applier;
    OllamaAdapter m_model;
    OllamaAdapter m_agentModel;
    PromptManager m_promptMgr;

    // Variables for tracking task execution time and context usage
    QTimer* m_taskTimer;
    QTime m_elapsedTime;  // Timer for measuring elapsed time
    int m_totalElapsedTime; // Total elapsed time in milliseconds
    QLabel* m_statusWidget;
    bool m_timerActive;
    bool m_taskRunning;  // Flag to track if a task is currently running
    QString m_currentPrompt;  // Current prompt being sent to the model
    
    // File system monitoring to track external file changes
    QFileSystemWatcher* m_fileWatcher;
    QMap<QString, QSet<QString>> m_fileToContextCommands; // Maps files to commands that created context from them
    QMap<QString, QString> m_shellCommandOutputs; // Maps shell commands to their output titles
    QMap<QString, QString> m_pythonScriptOutputs; // Maps python scripts to their output titles
    
    // New context management system
    ContextManager* m_contextManager;
    QWidget* m_contextStatusWidget; // Widget to display context command status
    QTreeWidget* m_contextCommandsList; // Tree to show which commands are in context with expandable details

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
    StyledButton* promptBtn;
    StyledButton* applyBtn;
    StyledButton* undoBtn;
    StyledButton* browseBtn;
    StyledButton* runSeqBtn;
    StyledButton *editSeqBtn;
    StyledButton *newSeqBtn;
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
    int                   m_currentStepIdx = -1; // -1 = не запущено


    void runNextStep();
    void runNextSequence();  // Run the next sequence specified by OPEN_SEQUENCE
    void addSequenceTransitionToHistory(const QString& sequenceName);  // Add sequence transition to history
    
    // Вспомогательная функция для суммаризации результатов поиска
    QString summarizeSearchResults(const QString& content, const QString& query);
    /* ────── NEW ────── */
    static PromptSequence defaultSequenceTemplate();   // шаблон по умолчанию
    void ensureDefaultSequence();                      // создаём, если нет
    void refreshAllSequences();                        // Refresh sequences from both PromptSequences and template folders
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
    StyledButton *settingsPanelBtn{nullptr};
    StyledButton *lightRagBtn{nullptr}; // кнопка «LightRAG»
    QWidget *m_settingsPanel{nullptr};
    GraphWidget  *m_graphWindow{nullptr};   // окно-виджет графа
    SequenceGraphWidget  *m_seqGraphWindow{nullptr};   // окно-виджет графа последовательностей
    LightRagWidget *m_lightRagWidget{nullptr}; // LightRAG widget
    TtsRunner *m_ttsRunner{nullptr}; // TTS Runner
    StyledButton *m_ttsToggleBtn{nullptr}; // TTS toggle button
    bool m_ttsEnabled{false}; // TTS enabled flag

    struct ContextRange {
        int startLine = -1;
        int endLine = -1;
        QStringList lines;
    };
    struct FileContextSlice {
        enum Mode { None, FullFile, Ranges } mode = None;
        QStringList fullFileLines;
        QVector<ContextRange> regions;
    };

    QMap<QString, FileContextSlice> m_contextSlices;
    QStringList m_consoleLogs;
    QStringList m_externalSources;
    ResultDisplayWidget* m_resultDisplayWidget = nullptr;

    void rebuildContextCache();
    QString formatNumberedLines(const QStringList& lines, int startLine) const;
    void setFullFileContext(const QString& path, const QStringList& lines);
    void addRegionContext(const QString& path, int startLine, const QStringList& lines);
    void clearContextForPath(const QString& path);
    bool applyGroupedEditInRange(const QList<Command*>& commands, QString* err);
    void handleOpenCommand(Command* cmd);
    void handlePostApplyContextCleanup(Command* cmd);
    void appendConsoleContext(const QString& title,
                              const QString& stdOut,
                              const QString& stdErr,
                              int exitCode);
    void appendExternalContext(const QString& block);
    void handleSearchResults(Command* cmd);

    void retryCurrentSequenceStep();
    bool ensureAgentModelConfigured();
    QStringList buildAllowedCommandsDocsCompact(const PromptStep& step) const;
    QByteArray repairToJsonArrayIfNeeded(const QByteArray& rawText, const PromptStep& step);
    bool maybeAutoRepairAfterExecution(const PromptStep& step,
                                       const QDateTime& execStartedAt,
                                       const QList<Command*>& executedCommands,
                                       bool applyFailed,
                                       const QString& lastError);
    static bool isImplementationIntentText(const QString& text);
    bool didAnyAffectedPathChangeSince(const QDateTime& since, const QList<Command*>& executedCommands) const;
    void appendUserQuestionToResultDisplay(const QString& question);
    ResultDisplayWidget::ResultEntryMetadata collectContextMetadataSince(const QDateTime &since) const;

private:
    void updateContextStatusDisplay(); // Update the visual display of context commands
    void addCommandToContext(Command* cmd, const QString& result = QString()); // Wrapper to handle command addition logic

    // Methods to manage the task status display
    void createStatusWidget();
    void updateStatusDisplay();
    void startTaskTimer();
    void stopTaskTimer();
    QString formatNumber(quint64 num) const;

};
