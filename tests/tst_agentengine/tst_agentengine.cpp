// The agent engine end to end: a scripted model answers, the engine checks the commands,
// asks the (scripted) user, runs them on a temporary project and moves through the steps.

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include "ScriptedModel.h"
#include "core/session/Session.h"
#include "core/workflow/AgentEngine.h"
#include "core/workflow/AgentHost.h"

namespace {

// The user, as the test wants them: accepts or declines, answers questions.
class ScriptedHost : public AgentHost
{
public:
    bool accept = true;
    QString answer = QStringLiteral("blue");
    int confirmations = 0;
    QStringList questions;

    void confirmCommands(const QList<Command*>& commands, const QString&,
                         std::function<void(const QList<Command*>&)> done) override
    {
        ++confirmations;
        done(accept ? commands : QList<Command*>());
    }
    void askUser(const QString& question, std::function<void(bool, const QString&)> done) override
    {
        questions << question;
        done(true, answer);
    }
};

PromptStep step(const QStringList& allowed)
{
    PromptStep s;
    s.setTemplate(QStringLiteral("Request: {{USER_QUERY}}\nCommands:\n{{SUPPORTED_COMMANDS}}"));
    s.setAllowedCommands(allowed);
    return s;
}

WorkflowLibrary::Found sequence(std::initializer_list<PromptStep> steps)
{
    PromptSequence seq(QStringLiteral("Test"));
    for (const PromptStep& s : steps)
        seq.addStep(s);
    return {seq, QStringLiteral("test.seq.json")};
}

struct Result
{
    QString title;
    QString content;
    AgentEngine::ResultKind kind;
};

} // namespace

class TestAgentEngine : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void runsCommandsAndShowsTheResult();
    void rejectsPathsOutsideTheProject();
    void declinedCommandsEndTheRun();
    void readOnlyCommandsRunWithoutAsking();
    void askUserWaitsForTheAnswer();
    void answerWithoutCommandsEndsWithASummary();
    void stopEndsTheRun();
    void chatStreamsTheAnswer();
    void modelErrorEndsTheRun();

private:
    bool runToEnd();
    QString read(const QString& relativePath) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Session> m_session;
    std::unique_ptr<ScriptedModel> m_model;
    std::unique_ptr<ScriptedHost> m_host;
    std::unique_ptr<AgentEngine> m_engine;
    QList<Result> m_results;
    QStringList m_finished;
};

void TestAgentEngine::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_session = std::make_unique<Session>();
    m_session->project().open(m_dir->path());
    QCOMPARE(m_session->project().root(), QDir::cleanPath(m_dir->path()));
    m_model = std::make_unique<ScriptedModel>();
    m_host = std::make_unique<ScriptedHost>();
    m_engine = std::make_unique<AgentEngine>(*m_session, *m_model, *m_host);
    m_results.clear();
    m_finished.clear();
    connect(m_engine.get(), &AgentEngine::resultReady, this,
            [this](const QString& title, const QString& content, AgentEngine::ResultKind kind) {
        m_results << Result{title, content, kind};
    });
    connect(m_engine.get(), &AgentEngine::runFinished, this, [this](const QString& message) { m_finished << message; });
    m_session->conversation().addUserMessage(QStringLiteral("create hello.txt"));
}

void TestAgentEngine::cleanup()
{
    m_engine.reset();
    m_host.reset();
    m_model.reset();
    m_session.reset();
    m_dir.reset();
}

bool TestAgentEngine::runToEnd()
{
    QSignalSpy finished(m_engine.get(), &AgentEngine::runFinished);
    return finished.count() > 0 || finished.wait(10000);
}

QString TestAgentEngine::read(const QString& relativePath) const
{
    QFile f(QDir(m_dir->path()).filePath(relativePath));
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

void TestAgentEngine::runsCommandsAndShowsTheResult()
{
    m_model->addAnswer(QStringLiteral(R"([{"command":"CREATE_FILE","params":{"path":"hello.txt","content":"hi\n"}}])"));
    m_model->addAnswer(QStringLiteral(R"([{"command":"SHOW_RESULT","params":{"title":"Done","content":"Created hello.txt"}}])"));

    m_engine->start(sequence({step({"CREATE_FILE"}), step({"SHOW_RESULT"})}));
    QVERIFY(runToEnd());

    QCOMPARE(read(QStringLiteral("hello.txt")), QStringLiteral("hi\n"));
    QCOMPARE(m_host->confirmations, 1);  // CREATE_FILE changes a file: asked once
    QCOMPARE(m_results.size(), 1);
    QCOMPARE(m_results.first().title, QStringLiteral("Done"));
    QVERIFY(m_results.first().content.startsWith(QStringLiteral("Created hello.txt")));
    QVERIFY(!m_engine->isRunning());
    QVERIFY(!m_session->run().isActive());

    // The prompt carried the request and the documentation of the step's commands.
    QCOMPARE(m_model->requests().size(), 2);
    const ModelRequest& first = m_model->requests().first();
    QCOMPARE(first.user, QStringLiteral("create hello.txt"));
    QVERIFY(first.system.contains(QStringLiteral("Request: create hello.txt")));
    QVERIFY(first.system.contains(QStringLiteral("CREATE_FILE")));
    QVERIFY(first.stream);
    QVERIFY(m_session->run().completedSteps.isEmpty());  // reset with the run
}

void TestAgentEngine::rejectsPathsOutsideTheProject()
{
    m_model->addAnswer(QStringLiteral(R"([{"command":"CREATE_FILE","params":{"path":"../evil.txt","content":"x"}}])"));
    m_model->addAnswer(QStringLiteral(R"([{"command":"SHOW_RESULT","params":{"content":"gave up"}}])"));

    m_engine->start(sequence({step({"CREATE_FILE"})}));
    QVERIFY(runToEnd());

    QVERIFY(!QFile::exists(QDir(m_dir->path()).filePath(QStringLiteral("../evil.txt"))));
    QCOMPARE(m_host->confirmations, 0);  // never shown to the user
    bool told = false;
    for (const ChatMessage& m : m_session->conversation().history())
        told = told || m.content.startsWith(QStringLiteral("Rejected commands:"));
    QVERIFY2(told, "the model is told why its command was rejected");
    QCOMPARE(m_model->requests().size(), 2);  // the same step was asked again
}

void TestAgentEngine::declinedCommandsEndTheRun()
{
    m_host->accept = false;
    m_model->addAnswer(QStringLiteral(R"([{"command":"CREATE_FILE","params":{"path":"hello.txt","content":"hi"}}])"));

    m_engine->start(sequence({step({"CREATE_FILE"}), step({"SHOW_RESULT"})}));
    QVERIFY(runToEnd());

    QVERIFY(!QFile::exists(QDir(m_dir->path()).filePath(QStringLiteral("hello.txt"))));
    QCOMPARE(m_finished.size(), 1);
    QVERIFY(!m_finished.first().isEmpty());
    QCOMPARE(m_model->requests().size(), 1);
}

void TestAgentEngine::readOnlyCommandsRunWithoutAsking()
{
    QFile f(QDir(m_dir->path()).filePath(QStringLiteral("notes.txt")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("line one\nline two\n");
    f.close();
    m_model->addAnswer(QStringLiteral(R"([{"command":"OPEN_FILE","params":{"path":"notes.txt"}}])"));
    m_model->addAnswer(QStringLiteral(R"([{"command":"SHOW_RESULT","params":{"content":"read it"}}])"));

    m_engine->start(sequence({step({"OPEN_FILE"}), step({"SHOW_RESULT"})}));
    QVERIFY(runToEnd());

    QCOMPARE(m_host->confirmations, 0);
    // The second prompt has the file's lines, numbered.
    QVERIFY(m_session->context().openFilesText().contains(QStringLiteral("1| line one")));
}

void TestAgentEngine::askUserWaitsForTheAnswer()
{
    m_model->addAnswer(QStringLiteral(R"([{"command":"ASK_USER","params":{"question":"Which color?"}}])"));
    m_model->addAnswer(QStringLiteral(R"([{"command":"SHOW_RESULT","params":{"content":"ok"}}])"));

    m_engine->start(sequence({step({"ASK_USER"}), step({"SHOW_RESULT"})}));
    QVERIFY(runToEnd());

    QCOMPARE(m_host->questions, QStringList{QStringLiteral("Which color?")});
    int answers = 0;
    for (const ChatMessage& m : m_session->conversation().history())
        answers += (m.role == QLatin1String("user") && m.content == QLatin1String("blue")) ? 1 : 0;
    QCOMPARE(answers, 1);
    // The answer is in the history, the request stays what it was.
    QCOMPARE(m_session->conversation().currentRequest(), QStringLiteral("create hello.txt"));
}

void TestAgentEngine::answerWithoutCommandsEndsWithASummary()
{
    m_model->addAnswer(QStringLiteral("I would rather talk about the weather."));

    m_engine->start(sequence({step({"SHOW_RESULT"})}));
    QVERIFY(runToEnd());

    // The sequence ended without SHOW_RESULT: the user still gets an answer.
    QCOMPARE(m_results.size(), 1);
    QCOMPARE(m_results.first().kind, AgentEngine::ResultKind::Answer);
    QVERIFY(m_results.first().content.contains(QStringLiteral("create hello.txt")));
}

void TestAgentEngine::stopEndsTheRun()
{
    m_model->addAnswer(QStringLiteral(R"([{"command":"CREATE_FILE","params":{"path":"late.txt","content":"x"}}])"));
    m_engine->start(sequence({step({"CREATE_FILE"})}));
    QVERIFY(m_engine->isRunning());
    m_engine->stop();
    QVERIFY(!m_engine->isRunning());
    QCOMPARE(m_finished.size(), 1);

    // The answer that was on its way changes nothing.
    QTest::qWait(100);
    QVERIFY(!QFile::exists(QDir(m_dir->path()).filePath(QStringLiteral("late.txt"))));
    QCOMPARE(m_finished.size(), 1);
}

void TestAgentEngine::chatStreamsTheAnswer()
{
    QStringList chunks;
    connect(m_engine.get(), &AgentEngine::answerChunk, this, [&chunks](const QString& c) { chunks << c; });
    m_model->addAnswer(QStringLiteral("Hello there"));

    m_engine->chat(QStringLiteral("hi"));
    QVERIFY(runToEnd());

    QCOMPARE(chunks.join(QString()), QStringLiteral("Hello there"));
    QVERIFY(chunks.size() > 1);
    QCOMPARE(m_session->conversation().lastAssistantMessage(), QStringLiteral("Hello there"));
    QCOMPARE(m_finished, QStringList{QString()});
}

void TestAgentEngine::modelErrorEndsTheRun()
{
    // No answer scripted: the model reports an error.
    m_engine->start(sequence({step({"SHOW_RESULT"})}));
    QVERIFY(runToEnd());
    QCOMPARE(m_finished.size(), 1);
    QVERIFY(m_finished.first().contains(QStringLiteral("no scripted answer left")));
    QVERIFY(!m_engine->isRunning());
}

QTEST_MAIN(TestAgentEngine)
#include "tst_agentengine.moc"
