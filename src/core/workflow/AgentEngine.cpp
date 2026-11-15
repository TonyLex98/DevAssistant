#include "core/workflow/AgentEngine.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QTimer>
#include "core/commands/CommandParser.h"
#include "core/commands/CommandPolicy.h"
#include "core/commands/SearchProjectCommand.h"
#include "core/commands/SearchWebCommand.h"
#include "core/session/Session.h"
#include "core/util/Logging.h"
#include "core/workflow/AgentHost.h"

namespace {

QList<Command*> raw(const QList<CommandPtr>& list)
{
    QList<Command*> out;
    for (const CommandPtr& c : list)
        out << c.get();
    return out;
}

bool contains(const QList<CommandPtr>& list, const char* name)
{
    for (const CommandPtr& c : list)
        if (c->name() == QLatin1String(name))
            return true;
    return false;
}

// Commands the engine handles itself instead of running them.
bool isHandledByEngine(const QString& name)
{
    static const QSet<QString> names = {
        QStringLiteral("ASK_USER"), QStringLiteral("OPEN_FILE"), QStringLiteral("OPEN_FILE_REGION"),
        QStringLiteral("SHOW_RESULT"), QStringLiteral("REASONING"), QStringLiteral("CONTEXT_SHOW"),
        QStringLiteral("CONTEXT_CLEAR"), QStringLiteral("CALL_SEQUENCE"), QStringLiteral("OPEN_SEQUENCE")
    };
    return names.contains(name);
}

// "Implement", "fix", "add"...: the user wants something changed, not just an answer.
bool asksForChanges(const QString& request)
{
    static const QStringList stems = {
        QStringLiteral("реализ"), QStringLiteral("сделай"), QStringLiteral("создай"), QStringLiteral("добав"),
        QStringLiteral("исправ"), QStringLiteral("почин"), QStringLiteral("внедр"), QStringLiteral("напис"),
        QStringLiteral("сгенер"), QStringLiteral("передел"), QStringLiteral("улучш"), QStringLiteral("доработ"),
        QStringLiteral("перепиш"),
        QStringLiteral("implement"), QStringLiteral("create"), QStringLiteral("add"), QStringLiteral("fix"),
        QStringLiteral("repair"), QStringLiteral("refactor"), QStringLiteral("rewrite"), QStringLiteral("build"),
        QStringLiteral("generate")
    };
    const QString lower = request.toLower();
    for (const QString& s : stems)
        if (lower.contains(s))
            return true;
    return false;
}

// Did any file a command names change after `since`?
bool touchedFilesChanged(const QList<CommandPtr>& commands, const QString& root, const QDateTime& since)
{
    static const char* const keys[] = {"path", "oldPath", "newPath", "folderPath", "scriptPath", "sourceFilePath"};
    for (const CommandPtr& c : commands) {
        const QJsonObject params = c->params();
        for (const char* key : keys) {
            const QString value = params.value(QLatin1String(key)).toString().trimmed();
            if (value.isEmpty())
                continue;
            const QFileInfo info(QFileInfo(value).isAbsolute() ? value : QDir(root).absoluteFilePath(value));
            if (info.exists() && info.lastModified() >= since)
                return true;
        }
    }
    return false;
}

QString stepSummary(const QList<CommandPtr>& batch)
{
    QStringList parts;
    for (const CommandPtr& c : batch) {
        const QJsonObject p = c->params();
        QString one = c->name();
        if (p.contains(QStringLiteral("path")))
            one += QStringLiteral(" path=%1").arg(p.value(QStringLiteral("path")).toString());
        if (p.contains(QStringLiteral("from")) && p.contains(QStringLiteral("to")))
            one += QStringLiteral(" %1→%2").arg(p.value(QStringLiteral("from")).toString(), p.value(QStringLiteral("to")).toString());
        parts << one;
    }
    return parts.join(QStringLiteral("; "));
}

} // namespace

AgentEngine::AgentEngine(Session& session, ModelClient& model, AgentHost& host, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_model(model)
    , m_host(host)
    , m_runner(session.project().manager())
{
}

AgentEngine::~AgentEngine()
{
    ++m_runId;
    m_runner.cancel();
    if (m_reply)
        m_reply->abort();
}

AgentEngine::ConfirmMode AgentEngine::confirmModeFromString(const QString& name)
{
    if (name == QLatin1String("auto"))
        return ConfirmMode::Never;
    if (name == QLatin1String("ask"))
        return ConfirmMode::Always;
    return ConfirmMode::ChangesOnly;
}

bool AgentEngine::isRunning() const
{
    return m_session.run().isActive() || m_chatting;
}

// ---------------------------------------------------------------- starting and stopping

void AgentEngine::start(const WorkflowLibrary::Found& sequence, const QString& templatePath)
{
    if (isRunning())
        endRun(QString());
    ++m_runId;
    RunState& run = m_session.run();
    run = RunState();
    run.sequence = sequence.sequence;
    run.sequencePath = sequence.path;
    run.templatePath = templatePath;
    run.stepIndex = 0;
    run.contextSequenceId = m_session.context().commands().startCommandSequence(
            run.sequence.name(), run.sequence.description());
    m_guard.reset();
    m_repairsThisStep = 0;
    m_inRepair = false;
    emit runStarted();
    runStep();
}

void AgentEngine::chat(const QString& text)
{
    if (isRunning())
        endRun(QString());
    ++m_runId;
    m_chatting = true;
    m_session.conversation().addUserMessage(text);
    emit runStarted();

    ModelRequest request;
    request.user = text;
    request.imagesBase64 = m_session.conversation().images().base64;
    request.stream = true;
    request.timeoutMs = 0;
    emit promptSent(text);
    ask(request,
        [this](const QString& answer) {
            m_chatting = false;
            m_session.conversation().addAssistantMessage(answer);
            emit answerFinished();
            emit runFinished(QString());
        },
        [this](const QString& error) {
            m_chatting = false;
            emit answerFinished();
            emit runFinished(tr("The model did not answer: %1").arg(error));
        });
}

void AgentEngine::stop()
{
    if (!isRunning())
        return;
    emit log(tr("Template execution stopped by the user."));
    endRun(tr("Template execution was stopped by the user."));
}

void AgentEngine::endRun(const QString& message)
{
    ++m_runId;  // whatever is still on its way belongs to a finished run
    m_runner.cancel();
    if (m_reply)
        m_reply->abort();
    m_reply = nullptr;
    RunState& run = m_session.run();
    if (run.isActive() && !run.contextSequenceId.isEmpty())
        m_session.context().commands().endCommandSequence(run.contextSequenceId);
    run = RunState();
    const bool wasChatting = m_chatting;
    m_chatting = false;
    if (wasChatting)
        emit answerFinished();
    emit stepChanged();
    emit runFinished(message);
}

// ---------------------------------------------------------------- talking to the model

void AgentEngine::ask(const ModelRequest& request, Callback onAnswer,
                      std::function<void(const QString&)> onError)
{
    if (m_reply)
        m_reply->abort();
    ModelReply* reply = m_model.send(request);
    m_reply = reply;
    const int runId = m_runId;
    if (request.stream)
        connect(reply, &ModelReply::chunk, this, &AgentEngine::answerChunk);
    connect(reply, &ModelReply::finished, this, [this, reply, runId, onAnswer, onError]() {
        reply->deleteLater();
        if (runId != m_runId || reply->isCancelled())
            return;
        if (m_reply == reply)
            m_reply = nullptr;
        if (reply->hasError()) {
            emit log(tr("Model error: %1").arg(reply->errorString()));
            if (onError)
                onError(reply->errorString());
            else
                endRun(tr("The model did not answer: %1").arg(reply->errorString()));
            return;
        }
        onAnswer(reply->text());
    });
}

void AgentEngine::scheduleStep(int delayMs)
{
    const int runId = m_runId;
    QTimer::singleShot(delayMs, this, [this, runId]() {
        if (runId == m_runId)
            runStep();
    });
}

void AgentEngine::runStep()
{
    RunState& run = m_session.run();
    if (!run.isActive())
        return;
    const PromptStep* step = run.currentStep();
    if (!step) {
        finishSequence();
        return;
    }
    m_inRepair = false;

    const PromptResolver::ResolvedPrompt prompt = PromptResolver::resolve(*step, m_session);
    const int tokens = m_model.estimateTokens(prompt.systemPrompt + QLatin1Char('\n') + prompt.userPrompt);
    emit log(QStringLiteral("[Step %1] Context ≈ %2 tokens").arg(run.stepIndex + 1).arg(tokens));
    emit log(QStringLiteral("\n--- Step %1 ------------------------------------------------").arg(run.stepIndex + 1));
    if (!prompt.userPrompt.isEmpty())
        emit log(QStringLiteral("USER_QUERY:\n%1").arg(prompt.userPrompt));
    m_session.conversation().addUserMessage(prompt.userPrompt);
    emit promptSent(prompt.systemPrompt + QLatin1Char('\n') + prompt.userPrompt);
    emit stepChanged();

    if (m_agentMode)
        askAgentModel(prompt);
    else
        askChatModel(prompt);
}

void AgentEngine::askChatModel(const PromptResolver::ResolvedPrompt& prompt)
{
    ModelRequest request;
    request.system = prompt.systemPrompt;
    request.user = prompt.userPrompt;
    request.imagesBase64 = m_session.conversation().images().base64;
    request.stream = true;
    request.timeoutMs = 0;
    ask(request, [this](const QString& answer) {
        emit answerFinished();
        handleAnswer(answer);
    });
}

void AgentEngine::askAgentModel(const PromptResolver::ResolvedPrompt& prompt)
{
    // First a short plan in prose, then the commands with the plan in front of the model.
    ModelRequest draft;
    draft.system = QStringLiteral("You are a senior planner.\n"
                                  "Return a short plan (5-10 bullets) and, if needed, 1-3 clarifying questions.\n"
                                  "Do NOT output JSON commands in this step.\n");
    draft.user = prompt.userPrompt;
    draft.options = {{QStringLiteral("temperature"), 0.5}, {QStringLiteral("num_predict"), 600}};
    draft.timeoutMs = 60000;

    const auto fallBack = [this, prompt](const QString& error) {
        emit log(tr("Agent mode: the planning call failed (%1); asking for the commands directly.").arg(error));
        askChatModel(prompt);
    };
    ask(draft, [this, prompt, fallBack](const QString& plan) {
        if (!plan.trimmed().isEmpty())
            m_session.context().addConsoleOutput(QStringLiteral("AGENT_RAW_DRAFT"), plan.trimmed(), QString(), 0);
        ModelRequest compile;
        compile.system = prompt.systemPrompt;
        if (!plan.trimmed().isEmpty())
            compile.system += QStringLiteral("\n\n[RAW_DRAFT]\n") + plan.trimmed() + QStringLiteral("\n\n");
        compile.system += QStringLiteral("\n\nIMPORTANT: Output ONLY a JSON array of commands. No extra text.\n");
        compile.user = prompt.userPrompt;
        compile.options = {{QStringLiteral("temperature"), 0.1}, {QStringLiteral("num_predict"), 1400}};
        compile.timeoutMs = 90000;
        ask(compile, [this](const QString& answer) { handleAnswer(answer); }, fallBack);
    }, fallBack);
}

void AgentEngine::repairJson(const QString& answer, std::function<void(const QByteArray&)> then)
{
    const PromptStep* step = m_session.run().currentStep();
    ModelRequest request;
    request.system = QStringLiteral(
            "You are a strict JSON repair tool.\n"
            "Task: Convert the INPUT into a valid JSON array of commands.\n"
            "Rules:\n"
            "- Output ONLY a JSON array. No markdown, no comments.\n"
            "- Each element is an object with keys: command, params.\n"
            "- params MUST be a JSON object (not a string).\n"
            "- Only use commands from the allowed list below.\n"
            "- If there is not enough info to act, output exactly one ASK_USER command.\n"
            "- If no actions are needed, output [].\n\n"
            "Allowed commands:\n%1\n")
            .arg(PromptResolver::commandDocs(step ? step->allowedCommands() : QStringList()));
    request.user = answer;
    request.options = {{QStringLiteral("temperature"), 0.05}, {QStringLiteral("num_predict"), 800}};
    request.timeoutMs = 45000;
    ask(request,
        [then](const QString& text) { then(CommandParser::extractJson(text.toUtf8())); },
        [then](const QString&) { then(QByteArray()); });
}

// ---------------------------------------------------------------- reading the answer

void AgentEngine::handleAnswer(const QString& text)
{
    if (!m_session.run().isActive())
        return;
    const QByteArray json = CommandParser::extractJson(text.toUtf8());
    if (json.isEmpty() && m_agentMode) {
        repairJson(text, [this, text](const QByteArray& repaired) { handleCommands(text, repaired); });
        return;
    }
    handleCommands(text, json);
}

void AgentEngine::handleCommands(const QString& answer, const QByteArray& json)
{
    const bool usedRaw = json.isEmpty();
    const QByteArray payload = usedRaw ? answer.toUtf8() : json;
    if (usedRaw) {
        addConsole(QStringLiteral("LLM RAW OUTPUT"), answer);
        emit log(tr("No JSON found in the model's answer; it was saved as context. Trying to parse the raw answer."));
    }
    m_session.conversation().addAssistantMessage(QString::fromUtf8(payload));

    QList<CommandPtr> commands;
    for (Command* c : CommandParser::parse(payload))
        commands << CommandPtr(c);
    if (commands.isEmpty()) {
        if (!usedRaw)
            addConsole(QStringLiteral("LLM RAW OUTPUT"), QString::fromUtf8(payload));
        emit log(tr("The model returned no commands. The raw answer was added to the context."));
        if (m_guard.emptyBatch()) {
            finishWithSummary(QStringLiteral("the model returned no commands for several steps in a row"), {});
            return;
        }
        advance(false);
        return;
    }

    // The policy first: a path outside the project, or a command the step does not allow,
    // never reaches the user or the disk. The model is told why.
    const PromptStep* step = m_session.run().currentStep();
    const QStringList allowed = step ? step->allowedCommands() : QStringList();
    const QString root = m_session.project().root();
    QList<CommandPtr> candidates;
    QStringList rejections;
    for (const CommandPtr& c : commands) {
        QString reason;
        if (CommandPolicy::check(c.get(), root, allowed, &reason))
            candidates << c;
        else
            rejections << reason;
    }
    if (!rejections.isEmpty()) {
        const QString details = QStringLiteral("\n- ") + rejections.join(QStringLiteral("\n- "));
        emit log(tr("Rejected commands:") + details);
        const QString feedback = QStringLiteral("Rejected commands:") + details;  // the model reads English
        m_session.conversation().addNote(feedback);
        addConsole(QStringLiteral("REJECTED COMMANDS"), feedback);
    }
    if (candidates.isEmpty()) {
        if (m_guard.emptyBatch())
            finishWithSummary(tr("the model keeps proposing commands that are not allowed"), {});
        else
            scheduleStep();  // the same step again, with the reasons in the history
        return;
    }
    confirmAndRun(candidates);
}

void AgentEngine::confirmAndRun(const QList<CommandPtr>& candidates)
{
    const bool askUser = m_confirmMode == ConfirmMode::Always
            || (m_confirmMode == ConfirmMode::ChangesOnly && !CommandPolicy::isReadOnlyBatch(raw(candidates)));
    if (!askUser) {
        runConfirmed(candidates);
        return;
    }
    const int runId = m_runId;
    m_host.confirmCommands(raw(candidates), m_session.project().root(),
                           [this, runId, candidates](const QList<Command*>& accepted) {
        if (runId != m_runId)
            return;
        QList<CommandPtr> batch;
        for (const CommandPtr& c : candidates)
            if (accepted.contains(c.get()))
                batch << c;
        if (batch.isEmpty()) {
            emit log(tr("The commands were declined."));
            endRun(tr("The commands were declined."));
            return;
        }
        runConfirmed(batch);
    });
}

void AgentEngine::runConfirmed(const QList<CommandPtr>& batch)
{
    const QString stopReason = m_guard.check(raw(batch));
    const int runId = m_runId;
    handleInteractive(batch, 0, [this, runId, batch, stopReason]() {
        if (runId != m_runId)
            return;
        if (!stopReason.isEmpty()) {
            finishWithSummary(stopReason, batch);
            return;
        }
        for (const CommandPtr& c : batch) {
            const bool call = c->name() == QLatin1String("CALL_SEQUENCE");
            if (!call && c->name() != QLatin1String("OPEN_SEQUENCE"))
                continue;
            const QJsonObject p = c->params();
            if (switchSequence(p.value(QStringLiteral("sequenceName")).toString(),
                               call && p.value(QStringLiteral("summarize")).toBool(false)))
                return;
        }
        runCommands(batch);
    });
}

// Commands about the conversation and the context, in the order the model gave them.
// ASK_USER waits for the answer before the rest go on.
void AgentEngine::handleInteractive(const QList<CommandPtr>& batch, int index, std::function<void()> then)
{
    RunState& run = m_session.run();
    WorkingContext& context = m_session.context();
    Conversation& conversation = m_session.conversation();
    for (; index < batch.size(); ++index) {
        Command* c = batch.at(index).get();
        const QString name = c->name();
        const QJsonObject p = c->params();

        if (name == QLatin1String("ASK_USER")) {
            const int runId = m_runId;
            m_host.askUser(p.value(QStringLiteral("question")).toString(),
                           [this, runId, batch, index, then](bool answered, const QString& answer) {
                if (runId != m_runId)
                    return;
                if (answered)
                    m_session.conversation().addUserReply(answer);
                handleInteractive(batch, index + 1, then);
            });
            return;
        }
        if (name == QLatin1String("OPEN_FILE") || name == QLatin1String("OPEN_FILE_REGION")) {
            context.open(c, run.contextSequenceId);
        } else if (name == QLatin1String("SHOW_RESULT")) {
            QString title = p.value(QStringLiteral("title")).toString();
            if (title.isEmpty())
                title = tr("Answer from %1:").arg(m_model.model()) + QStringLiteral("\n\n");
            QString content = p.value(QStringLiteral("content")).toString();
            const QString spoken = content;
            content = content.isEmpty() ? tr("SHOW_RESULT was called without text.") + QStringLiteral("\n\n\n")
                                        : content + QStringLiteral("\n\n\n");
            showResult(title, content, ResultKind::Answer);
            m_guard.resultShown();
            QString speech = spoken;
            speech.replace(QStringLiteral("\\n"), QStringLiteral(" ")).replace(QStringLiteral("\\r"), QString())
                  .replace(QLatin1Char('\n'), QLatin1Char(' '));
            if (speech.length() > 5)
                emit speak(speech.length() > 1000 ? speech.left(1000) + QStringLiteral("...") : speech);
            context.record(c, c->getResult(), run.contextSequenceId);
        } else if (name == QLatin1String("REASONING")) {
            QString reasoning = p.value(QStringLiteral("reasoning")).toString();
            if (reasoning.isEmpty())
                reasoning = p.value(QStringLiteral("thought_process")).toString();
            if (reasoning.isEmpty())
                reasoning = p.value(QStringLiteral("reasoning_text")).toString();
            if (!reasoning.isEmpty())
                showResult(QString(), reasoning, ResultKind::Reasoning);
            context.record(c, c->getResult(), run.contextSequenceId);
        } else if (name == QLatin1String("CONTEXT_SHOW")) {
            const int maxChars = qMax(0, p.value(QStringLiteral("maxChars")).toInt(8000));
            QString text = context.openFilesText();
            if (maxChars > 0 && text.size() > maxChars)
                text = text.left(maxChars) + QStringLiteral("\n[...truncated...]");
            const QString result = QStringLiteral("CONTEXT_SHOW\n[context]\n%1").arg(text.trimmed());
            showResult(tr("Context"), result, ResultKind::Context);
            conversation.addNote(result);
            context.record(c, result, run.contextSequenceId);
        } else if (name == QLatin1String("CONTEXT_CLEAR")) {
            const QString scope = p.value(QStringLiteral("scope")).toString().trimmed().toLower();
            context.clear(scope == QLatin1String("all"));
            const QString result = QStringLiteral("CONTEXT_CLEAR\n[scope]\n%1\n[status]\nOK")
                                           .arg(scope.isEmpty() ? QStringLiteral("active") : scope);
            conversation.addNote(result);
            context.record(c, result, run.contextSequenceId);
        }
    }
    then();
}

// ---------------------------------------------------------------- running the commands

void AgentEngine::runCommands(const QList<CommandPtr>& batch)
{
    RunState& run = m_session.run();
    const QString summary = stepSummary(batch);
    if (!summary.isEmpty())
        run.completedSteps << QStringLiteral("Step %1: %2").arg(run.stepIndex + 1).arg(summary);

    QList<CommandPtr> runnable;
    for (const CommandPtr& c : batch)
        if (!isHandledByEngine(c->name()))
            runnable << c;

    m_batchStartedAt = QDateTime::currentDateTime();
    const int runId = m_runId;
    m_runner.run(runnable,
                 [this](Command* c) { afterCommand(c); },
                 [this, runId, batch](const CommandRunner::Outcome& outcome) {
                     if (runId == m_runId)
                         afterBatch(outcome, batch);
                 });
}

void AgentEngine::afterCommand(Command* c)
{
    WorkingContext& context = m_session.context();
    const QString sequenceId = m_session.run().contextSequenceId;
    const QString result = c->getResult();
    if (!result.isEmpty()) {
        context.addConsoleOutput(result.section(QLatin1Char('\n'), 0, 0), result, QString(), c->wasSuccessful() ? 0 : 1);
        m_session.conversation().addNote(result);
    } else if (!c->wasSuccessful()) {
        // The model learns why its command failed and can do better in the next step.
        m_session.conversation().addNote(QStringLiteral("%1 failed: %2").arg(c->name(), c->getErrorMessage()));
    }
    context.forgetChangedFiles(c);

    if (auto* web = dynamic_cast<SearchWebCommand*>(c)) {
        if (!web->results().isEmpty()) {
            const QString block = QStringLiteral("==== SEARCH_WEB: %1 ====\n%2")
                                          .arg(web->query(), web->results().join(QStringLiteral("\n\n")));
            context.addExternal(block);
            context.record(c, block, sequenceId);
            return;
        }
    } else if (auto* search = dynamic_cast<SearchProjectCommand*>(c)) {
        if (!search->matches().isEmpty()) {
            const QString block = QStringLiteral("==== SEARCH_PROJECT: %1 ====\n%2")
                                          .arg(c->params().value(QStringLiteral("query")).toString(),
                                               search->matches().join(QStringLiteral("\n\n")));
            context.addExternal(block);
            context.record(c, block, sequenceId);
            return;
        }
    }
    context.record(c, result, sequenceId);
}

void AgentEngine::afterBatch(const CommandRunner::Outcome& outcome, const QList<CommandPtr>& batch)
{
    if (!outcome.executed.isEmpty())
        emit projectChanged();
    if (outcome.failed) {
        emit log(tr("Execution error: %1").arg(outcome.error));
        showResult(tr("Execution error"), outcome.error, ResultKind::Error);
    }

    QList<CommandPtr> searches;
    for (const CommandPtr& c : outcome.executed) {
        auto* web = dynamic_cast<SearchWebCommand*>(c.get());
        if (web && c->wasSuccessful() && !web->summarySource().isEmpty())
            searches << c;
    }
    const int runId = m_runId;
    summarizeSearches(searches, [this, runId, outcome, batch]() {
        if (runId != m_runId)
            return;
        if (tryRepair(outcome, batch))
            return;
        const RunState& run = m_session.run();
        const PromptStep* step = run.currentStep();
        if (!step) {
            finishSequence();
            return;
        }
        const bool lastStep = !step->loop() && run.stepIndex + 1 >= run.sequence.steps().size();
        if (lastStep && !contains(batch, "SHOW_RESULT") && !m_guard.hasShownResult()) {
            finishWithSummary(QStringLiteral("the final step returned no SHOW_RESULT"), batch);
            return;
        }
        advance(step->loop());
    });
}

// The pages a web search found, summarized by the model for the next steps.
void AgentEngine::summarizeSearches(QList<CommandPtr> searches, std::function<void()> then)
{
    if (searches.isEmpty() || m_model.model().isEmpty()) {
        then();
        return;
    }
    const CommandPtr command = searches.takeFirst();
    auto* web = static_cast<SearchWebCommand*>(command.get());
    QString content = web->summarySource().trimmed();
    QString note;
    constexpr int kMaxChars = 12000;
    if (content.size() > kMaxChars) {
        content = content.left(kMaxChars);
        note = QStringLiteral("\n[Content truncated to %1 characters]").arg(kMaxChars);
    }
    const QString query = web->query().trimmed().isEmpty() ? QStringLiteral("(empty search query)") : web->query().trimmed();

    ModelRequest request;
    request.system = QStringLiteral(
            "You are a developer's assistant. Write a short, structured summary of the search results below. "
            "Give the key findings, links to the sources and recommended actions. "
            "Answer in the language of the user's request and do not make up facts.");
    request.user = QStringLiteral("Query: %1\nSearch results:\n%2%3\n\n"
                                  "Write the final summary (3-4 points or short paragraphs) with the key facts, links and possible actions.")
                           .arg(query, content, note);
    request.timeoutMs = 120000;
    const auto next = [this, searches, then]() { summarizeSearches(searches, then); };
    ask(request,
        [this, query, next](const QString& summary) {
            if (!summary.trimmed().isEmpty())
                m_session.context().addExternal(QStringLiteral("==== SEARCH_WEB SUMMARY: %1 ====\n%2").arg(query, summary.trimmed()));
            next();
        },
        [next](const QString&) { next(); });
}

// Agent mode: when the commands failed, or the user asked for changes and nothing changed,
// ask once more for commands that make progress (twice per step at most).
bool AgentEngine::tryRepair(const CommandRunner::Outcome& outcome, const QList<CommandPtr>& batch)
{
    Q_UNUSED(batch)
    if (!m_agentMode || m_inRepair || m_repairsThisStep >= 2)
        return false;
    const QString request = m_session.conversation().currentRequest();
    const bool changed = touchedFilesChanged(outcome.executed, m_session.project().root(), m_batchStartedAt);
    if (!outcome.failed && (changed || !asksForChanges(request)))
        return false;

    const PromptStep* step = m_session.run().currentStep();
    ModelRequest repair;
    repair.system = QStringLiteral(
            "You are an expert developer agent that must output ONLY a JSON array of commands.\n"
            "Goal: make real progress towards the USER REQUEST using the allowed commands.\n"
            "Rules:\n"
            "- Output ONLY JSON array, no extra text.\n"
            "- Use as few commands as possible (1-5).\n"
            "- Prefer file-editing commands when implementation is requested.\n"
            "- If missing critical info, output exactly one ASK_USER.\n"
            "- Do NOT loop on CALL_SEQUENCE.\n\n"
            "Allowed commands:\n%1\n")
            .arg(PromptResolver::commandDocs(step ? step->allowedCommands() : QStringList()));
    repair.user = QStringLiteral("USER REQUEST:\n%1\n").arg(request);
    if (outcome.failed && !outcome.error.trimmed().isEmpty())
        repair.user += QStringLiteral("\nLAST ERROR:\n%1\n").arg(outcome.error);
    const QString recent = m_session.context().activeCommandsText();
    if (!recent.trimmed().isEmpty())
        repair.user += QStringLiteral("\nRECENT CONTEXT (executed commands/results):\n%1\n").arg(recent.left(12000));
    repair.options = {{QStringLiteral("temperature"), 0.1}, {QStringLiteral("num_predict"), 1200}};
    repair.timeoutMs = 60000;

    ++m_repairsThisStep;
    m_inRepair = true;
    ask(repair,
        [this](const QString& text) {
            const QByteArray json = CommandParser::extractJson(text.toUtf8());
            if (json.isEmpty()) {
                advance(false);
                return;
            }
            addConsole(QStringLiteral("AGENT_REPAIR_DRAFT"), text);
            handleCommands(text, json);
        },
        [this](const QString&) { advance(false); });
    return true;
}

// ---------------------------------------------------------------- moving on

bool AgentEngine::switchSequence(const QString& name, bool summarize)
{
    RunState& run = m_session.run();
    const WorkflowLibrary::Found found = m_session.workflows().find(name, run.sequencePath);
    if (!found.isValid()) {
        emit log(tr("Sequence not found: %1").arg(name));
        return false;
    }
    if (summarize) {
        QStringList done;
        for (int i = 0; i < run.stepIndex && i < run.sequence.steps().size(); ++i) {
            QString first = run.sequence.steps().at(i).tmpl().section(QLatin1Char('\n'), 0, 0).trimmed();
            if (first.length() > 80)
                first = first.left(80) + QStringLiteral("...");
            done << QStringLiteral("Step %1: %2").arg(i + 1).arg(first);
        }
        if (!done.isEmpty())
            m_session.conversation().addNote(QStringLiteral("The previous sequence completed: %1").arg(done.join(QStringLiteral("; "))));
    }
    ContextManager& history = m_session.context().commands();
    if (!run.contextSequenceId.isEmpty())
        history.endCommandSequence(run.contextSequenceId);
    run.sequence = found.sequence;
    run.sequencePath = found.path;
    run.stepIndex = 0;
    run.contextSequenceId = history.startCommandSequence(found.sequence.name(), found.sequence.description());
    m_repairsThisStep = 0;
    m_session.conversation().addNote(QStringLiteral("Switched to sequence: %1").arg(name));
    emit stepChanged();
    scheduleStep();
    return true;
}

void AgentEngine::advance(bool loopStep)
{
    RunState& run = m_session.run();
    if (!run.isActive())
        return;
    if (!loopStep) {
        ++run.stepIndex;
        m_repairsThisStep = 0;
    }
    scheduleStep();
}

void AgentEngine::finishSequence()
{
    if (!m_guard.hasShownResult())
        finishWithSummary(QStringLiteral("the sequence ended without SHOW_RESULT"), {});
    else
        endRun(tr("The sequence has finished."));
}

// The run stops itself; the user gets what was found instead of nothing.
void AgentEngine::finishWithSummary(const QString& reason, const QList<CommandPtr>& recent)
{
    emit log(QStringLiteral("Finished automatically: %1").arg(reason));
    QStringList lines{tr("I stopped automatically so as not to run in a loop.")};
    const QString request = m_session.conversation().currentRequest().trimmed();
    if (!request.isEmpty())
        lines << tr("Request: %1").arg(request);

    QStringList sources;
    QStringList names;
    for (const CommandPtr& c : recent) {
        names << c->name();
        auto* web = dynamic_cast<SearchWebCommand*>(c.get());
        if (!web)
            continue;
        for (const QString& block : web->results()) {
            if (sources.size() >= 5)
                break;
            const QStringList parts = block.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            if (parts.size() >= 2 && parts.at(1).trimmed().startsWith(QLatin1String("http")))
                sources << QStringLiteral("%1 - %2").arg(parts.at(0).trimmed(), parts.at(1).trimmed());
        }
    }
    if (!sources.isEmpty()) {
        lines << tr("Sources found:");
        for (const QString& s : sources)
            lines << QStringLiteral("- ") + s;
    } else if (!names.isEmpty()) {
        lines << tr("Last commands run: %1").arg(names.join(QStringLiteral(", ")));
    }
    lines << tr("If needed, make the request a little more specific: period, region, sources.");
    showResult(tr("Model's answer"), lines.join(QStringLiteral("\n\n")) + QStringLiteral("\n\n\n"), ResultKind::Answer);
    m_guard.resultShown();
    endRun(QString());
}

void AgentEngine::showResult(const QString& title, const QString& content, ResultKind kind)
{
    emit resultReady(title, content, kind);
}

void AgentEngine::addConsole(const QString& title, const QString& text)
{
    m_session.context().addConsoleOutput(title, text, QString(), -1);
}
