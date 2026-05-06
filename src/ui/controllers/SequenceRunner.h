#pragma once

#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
#include <QDateTime>

class MainWindow;
class PromptSequence;
class PromptStep;
class Command;
class OllamaAdapter;

/*
 * SequenceRunner — orchestrates execution of prompt sequences.
 *
 * Extracted from MainWindow to isolate the ~2200-line sequence execution
 * loop (runSequenceDialog → runNextStep → handleAssistantFinished → …).
 *
 * The runner holds its own execution state (step index, stability counters,
 * stop-flag) and delegates back to MainWindow for:
 *   - UI operations (QMessageBox, QInputDialog, CommandConfirmDialog)
 *   - Model calls (OllamaAdapter streaming / generation)
 *   - Command application (DevAssManager, ChangeApplier)
 *   - Logging, rescan, context-cache updates
 */
class SequenceRunner
{
public:
    explicit SequenceRunner(MainWindow* mw);

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

private:
    MainWindow* m_mw;

    // Execution state (moved from MainWindow)
    bool m_resultShownInCurrentRun{false};
    int m_batchesWithoutResult{0};
    int m_sameBatchRepeatCount{0};
    QString m_lastBatchSignature;
};
