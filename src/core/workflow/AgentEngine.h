#pragma once
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include "core/context/PromptResolver.h"
#include "core/model/ModelClient.h"
#include "core/workflow/CommandRunner.h"
#include "core/workflow/LoopGuard.h"
#include "core/workflow/WorkflowLibrary.h"

class AgentHost;
class LlmClient;
class Session;

// Runs prompt sequences: for each step it fills the prompt, asks the model, reads the commands
// in the answer, checks them against the policy, asks the user to confirm changes, runs them
// and gives the results back to the model, then moves to the next step.
//
// Everything is asynchronous: the engine never waits, so the window stays responsive and
// "Stop" works at any moment. Each run has a number; answers that arrive for a stopped run
// are dropped.
class AgentEngine : public QObject
{
    Q_OBJECT
public:
    // When the user is asked before commands run.
    enum class ConfirmMode {
        ChangesOnly,  // read-only batches run at once (the default)
        Always,
        Never
    };
    Q_ENUM(ConfirmMode)
    // What a result shown to the user is.
    enum class ResultKind { Answer, Reasoning, Context, Error };
    Q_ENUM(ResultKind)

    AgentEngine(Session& session, AgentHost& host, QObject* parent = nullptr);
    ~AgentEngine() override;

    void setConfirmMode(ConfirmMode mode) { m_confirmMode = mode; }
    ConfirmMode confirmMode() const { return m_confirmMode; }
    static ConfirmMode confirmModeFromString(const QString& name);  // "readonly", "ask", "auto"
    // Agent mode: a planning call before each step, JSON repair and a repair attempt when a
    // step changed nothing, all with the agent model. Off by default.
    void setAgentMode(bool on) { m_agentMode = on; }
    bool agentMode() const { return m_agentMode; }

    // Starts a sequence for the current request (Session::conversation().currentRequest()).
    void start(const WorkflowLibrary::Found& sequence, const QString& templatePath = QString());
    // A plain chat message: no sequence and no commands; the answer is streamed.
    void chat(const QString& text);
    void stop();
    bool isRunning() const;

signals:
    void log(const QString& line);
    // Streamed text of the model's answer, for the chat panel.
    void answerChunk(const QString& text);
    void answerFinished();
    void resultReady(const QString& title, const QString& content, AgentEngine::ResultKind kind);
    void runStarted();
    // The run ended; `message` says why (empty if a result was shown).
    void runFinished(const QString& message);
    // The running sequence or step changed.
    void stepChanged();
    // Commands changed files in the project.
    void projectChanged();
    // The text sent to the model, for the token estimate in the status bar.
    void promptSent(const QString& prompt);
    // A result worth reading aloud.
    void speak(const QString& text);

private:
    using Callback = std::function<void(const QString& text)>;

    // One model call; `onAnswer` gets the text, a failure ends the run unless `onError` is given.
    void ask(LlmClient& model, const ModelRequest& request, Callback onAnswer,
             std::function<void(const QString& error)> onError = nullptr);
    void runStep();
    void scheduleStep(int delayMs = 200);
    void askAgentModel(const PromptResolver::ResolvedPrompt& prompt);
    void askChatModel(const PromptResolver::ResolvedPrompt& prompt);
    void handleAnswer(const QString& text);
    void handleCommands(const QString& answer, const QByteArray& json);
    void confirmAndRun(const QList<CommandPtr>& candidates);
    void runConfirmed(const QList<CommandPtr>& batch);
    void handleInteractive(const QList<CommandPtr>& batch, int index, std::function<void()> then);
    void runCommands(const QList<CommandPtr>& batch);
    void afterCommand(Command* command);
    void afterBatch(const CommandRunner::Outcome& outcome, const QList<CommandPtr>& batch);
    void summarizeSearches(QList<CommandPtr> searches, std::function<void()> then);
    bool tryRepair(const CommandRunner::Outcome& outcome, const QList<CommandPtr>& batch);
    void repairJson(const QString& answer, std::function<void(const QByteArray& json)> then);
    bool switchSequence(const QString& name, bool summarize);
    void advance(bool loopStep);
    void finishSequence();
    void finishWithSummary(const QString& reason, const QList<CommandPtr>& recent);
    void endRun(const QString& message);
    void showResult(const QString& title, const QString& content, ResultKind kind);
    void addConsole(const QString& title, const QString& text);
    LlmClient& stepModel();

    Session& m_session;
    AgentHost& m_host;
    CommandRunner m_runner;
    LoopGuard m_guard;
    ConfirmMode m_confirmMode = ConfirmMode::ChangesOnly;
    bool m_agentMode = false;

    int m_runId = 0;               // incremented by every start and stop
    bool m_chatting = false;
    QPointer<ModelReply> m_reply;  // the request in flight
    int m_repairsThisStep = 0;
    bool m_inRepair = false;
    QDateTime m_batchStartedAt;
};
