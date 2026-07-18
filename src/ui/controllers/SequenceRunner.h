#pragma once

#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
#include <QDateTime>
#include "../../services/IService.h"

class PromptSequence;
class PromptStep;
class Command;
class MainWindow;

/*
 * SequenceRunner — orchestrates execution of prompt sequences.
 *
 * Extracted from MainWindow to isolate the ~1700-line sequence execution
 * loop (runSequenceDialog → runNextStep → handleAssistantFinished → …).
 *
 * The runner holds its own execution state (step index, stability counters,
 * stop-flag) and depends on interfaces for:
 *   - ISequenceService: sequence state, stop, agent pipeline
 *   - IContextService: context cache, file watcher, shell/python outputs
 *   - IModelService: model, agent model, stream buf, history
 *   - IProjectService: project root, dev manager
 *   - IUIUpdater: log, result display, status bar
 *   - ITemplateService: template manager
 *   - IStatusControllerService: status controller
 *   - ITTSService: TTS controller
 */
class SequenceRunner
{
public:
    SequenceRunner(MainWindow* mainWindow,
                   ISequenceService* seq, IContextService* ctx, IModelService* model,
                   IProjectService* project, IUIUpdater* ui, ITemplateService* tmpl,
                   IStatusControllerService* status, ITTSService* tts);

    // Entry points (replace MainWindow::runSequenceDialog / runNextStep / handleAssistantFinished)
    void runSequenceDialog();
    void runNextStep();
    void handleAssistantFinished();
    void retryCurrentSequenceStep();

    // Sequence transition
    void runNextSequence();
    void addSequenceTransitionToHistory(const QString& sequenceName);

    // Stability / auto-finalize
    void resetSequenceStabilityState();
    void autoFinalizeSequenceWithResult(const QString& reason, const QList<Command*>& recentCommands);
    void appendResultToUi(const QString& title, const QString& content);

    // Stop execution
    void stopExecution();

    // Agent pipeline helpers
    bool ensureAgentModelConfigured();
    QStringList buildAllowedCommandsDocsCompact(const PromptStep& step) const;
    QByteArray repairToJsonArrayIfNeeded(const QByteArray& rawText, const PromptStep& step);

    // Intent / path helpers
    static bool isImplementationIntentText(const QString& text);
    bool didAnyAffectedPathChangeSince(const QDateTime& since, const QList<Command*>& executedCommands) const;
    bool maybeAutoRepairAfterExecution(const PromptStep& step,
                                       const QDateTime& execStartedAt,
                                       const QList<Command*>& executedCommands,
                                       bool applyFailed,
                                       const QString& lastError);

    // Batch helpers
    QString commandBatchSignature(const QList<Command*>& commands) const;
    bool isReadOnlyCommandBatch(const QList<Command*>& commands) const;

    // Result display helpers
    void appendUserQuestionToResultDisplay(const QString& question);

    // Command handlers (extracted from MainWindow)
    void handleOpenCommand(Command* cmd);
    bool applyGroupedEditInRange(const QList<Command*>& commands, QString* err);
    void handlePostApplyContextCleanup(Command* cmd);
    void handleSearchResults(Command* cmd);
    void appendConsoleContext(const QString& title,
                              const QString& stdOut,
                              const QString& stdErr,
                              int exitCode);
    void appendExternalContext(const QString& block);

private:
    MainWindow* m_mainWindow;
    ISequenceService* m_seq;
    IContextService* m_ctx;
    IModelService* m_model;
    IProjectService* m_project;
    IUIUpdater* m_ui;
    ITemplateService* m_tmpl;
    IStatusControllerService* m_status;
    ITTSService* m_tts;

    // Execution state (moved from MainWindow)
    bool m_resultShownInCurrentRun{false};
    int m_batchesWithoutResult{0};
    int m_sameBatchRepeatCount{0};
    QString m_lastBatchSignature;
};
