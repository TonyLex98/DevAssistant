// The session and its parts, without a model: the conversation, the loop guard, finding
// sequences, filling prompts, the project and the working context.

#include <QDir>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include "TestFiles.h"
#include "core/commands/CommandRegistry.h"
#include "core/context/PromptResolver.h"
#include "core/session/Session.h"
#include "core/workflow/BuiltinWorkflows.h"
#include "core/workflow/LoopGuard.h"

namespace {

std::unique_ptr<Command> command(const char* json)
{
    return std::unique_ptr<Command>(CommandRegistry::create(QJsonDocument::fromJson(json).object()));
}

bool writeSequence(const QString& path, const QString& name)
{
    PromptSequence seq(name);
    PromptStep s;
    s.setTemplate(QStringLiteral("step of ") + name);
    seq.addStep(s);
    return TestFiles::write(path, QJsonDocument(seq.toJson()).toJson());
}

} // namespace

class TestSession : public QObject
{
    Q_OBJECT

private slots:
    void conversationKeepsRequestsAndReplies();
    void conversationHistoryText();
    void loopGuardStopsEmptyAnswers();
    void loopGuardStopsRepeatedSearches();
    void loopGuardResetsOnResult();
    void libraryFindsSequencesByNameAndFile();
    void libraryPrefersTheRunningTemplate();
    void libraryInstallsBuiltins();
    void promptResolverFillsPlaceholders();
    void promptResolverDropsUnknownPlaceholders();
    void projectListsFilesAndStructure();
    void workingContextKeepsNewestOutputs();
    void workingContextForgetsChangedFiles();
};

void TestSession::conversationKeepsRequestsAndReplies()
{
    Conversation c;
    c.addUserMessage(QStringLiteral("fix the build"));
    c.addUserMessage(QStringLiteral("fix the build"));  // the same request again is not repeated
    c.addAssistantMessage(QStringLiteral("[]"));
    c.addUserReply(QStringLiteral("use cmake"));
    c.addNote(QStringLiteral("CREATE_FILE failed"));

    QCOMPARE(c.currentRequest(), QStringLiteral("fix the build"));  // a reply does not replace it
    QCOMPARE(c.recentRequests(), (QStringList{"fix the build", "use cmake"}));
    QCOMPARE(c.lastAssistantMessage(), QStringLiteral("[]"));
    QCOMPARE(c.history().size(), 4);
    QCOMPARE(c.history().last().role, QStringLiteral("system"));
}

void TestSession::conversationHistoryText()
{
    Conversation c;
    for (int i = 0; i < 20; ++i)
        c.addNote(QStringLiteral("note %1 ").arg(i) + QString(100, QLatin1Char('x')));
    const QStringList lines = c.historyText(3, 20).split(QLatin1Char('\n'));
    QCOMPARE(lines.size(), 3);
    QVERIFY(lines.first().startsWith(QStringLiteral("system: note 17")));
    QVERIFY(lines.first().endsWith(QStringLiteral("...")));
}

void TestSession::loopGuardStopsEmptyAnswers()
{
    LoopGuard guard;
    for (int i = 0; i < 6; ++i)
        QVERIFY(!guard.emptyBatch());
    QVERIFY(guard.emptyBatch());
}

void TestSession::loopGuardStopsRepeatedSearches()
{
    LoopGuard guard;
    auto search = command(R"({"command":"SEARCH_PROJECT","params":{"query":"Foo"}})");
    QVERIFY(search);
    QVERIFY(guard.check({search.get()}).isEmpty());
    QVERIFY(guard.check({search.get()}).isEmpty());
    QVERIFY(!guard.check({search.get()}).isEmpty());  // the third identical search

    // Changing something is progress: no stop however often.
    LoopGuard other;
    auto create = command(R"({"command":"CREATE_FILE","params":{"path":"a.txt","content":"x"}})");
    for (int i = 0; i < 10; ++i)
        QVERIFY(other.check({create.get()}).isEmpty());
}

void TestSession::loopGuardResetsOnResult()
{
    LoopGuard guard;
    auto search = command(R"({"command":"SEARCH_PROJECT","params":{"query":"Foo"}})");
    auto result = command(R"({"command":"SHOW_RESULT","params":{"content":"done"}})");
    guard.check({search.get()});
    guard.check({search.get()});
    QVERIFY(guard.check({result.get()}).isEmpty());
    QVERIFY(guard.hasShownResult());
    QVERIFY(guard.check({search.get()}).isEmpty());  // counting starts again
    QVERIFY(guard.check({search.get()}).isEmpty());
    QVERIFY(!guard.emptyBatch());                    // a result was shown: never "give up"
}

void TestSession::libraryFindsSequencesByNameAndFile()
{
    QTemporaryDir dir;
    const QString sequences = dir.filePath(QStringLiteral("seq"));
    QVERIFY(writeSequence(sequences + QStringLiteral("/review.seq.json"), QStringLiteral("Code Review")));
    WorkflowLibrary library;
    library.setFolders(sequences, dir.filePath(QStringLiteral("tmpl")));

    for (const QString& name : {QStringLiteral("Code Review"), QStringLiteral("review"),
                                QStringLiteral("review.seq"), QStringLiteral("review.seq.json")}) {
        const WorkflowLibrary::Found f = library.find(name, QString());
        QVERIFY2(f.isValid(), qPrintable(name));
        QCOMPARE(f.sequence.name(), QStringLiteral("Code Review"));
    }
    QVERIFY(!library.find(QStringLiteral("missing"), QString()).isValid());
    QVERIFY(!library.find(QString(), QString()).isValid());
}

void TestSession::libraryPrefersTheRunningTemplate()
{
    QTemporaryDir dir;
    const QString sequences = dir.filePath(QStringLiteral("seq"));
    QVERIFY(writeSequence(sequences + QStringLiteral("/testing.seq.json"), QStringLiteral("Plain testing")));
    WorkflowLibrary library;
    library.setFolders(sequences, dir.filePath(QStringLiteral("tmpl")));

    // The Universal Agent template has its own testing.seq.json.
    const QString agentDir = dir.filePath(QStringLiteral("tmpl/UniversalAgent"));
    const WorkflowLibrary::Found f = library.find(QStringLiteral("testing"), agentDir + QStringLiteral("/main.seq.json"));
    QVERIFY(f.isValid());
    QCOMPARE(f.sequence.name(), QStringLiteral("Universal Agent Testing"));
    QCOMPARE(library.find(QStringLiteral("testing"), QString()).sequence.name(), QStringLiteral("Plain testing"));
}

void TestSession::libraryInstallsBuiltins()
{
    QTemporaryDir dir;
    WorkflowLibrary library;
    library.setFolders(dir.filePath(QStringLiteral("seq")), dir.filePath(QStringLiteral("tmpl")));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("seq/default.seq.json"))));

    QString error;
    const QString agent = QDir(dir.filePath(QStringLiteral("tmpl/UniversalAgent"))).absolutePath();
    const WorkflowLibrary::Found main = library.mainSequenceOf(agent, &error);
    QVERIFY2(main.isValid(), qPrintable(error));
    QCOMPARE(main.sequence.name(), QStringLiteral("Universal Agent Main"));
    QVERIFY(!library.mainSequenceOf(dir.filePath(QStringLiteral("nowhere")), &error).isValid());
    QVERIFY(!error.isEmpty());
    QCOMPARE(BuiltinWorkflows::defaultSequence().steps().size(), 5);
}

void TestSession::promptResolverFillsPlaceholders()
{
    QTemporaryDir dir;
    QVERIFY(TestFiles::write(dir.filePath(QStringLiteral("src/main.cpp")), "int main() {}\n"));
    Session session;
    session.project().open(dir.path());
    session.conversation().addUserMessage(QStringLiteral("explain main"));

    PromptStep step;
    step.setTemplate(QStringLiteral("Q={{USER_QUERY}}\nROOT={{PROJECT_ROOT}}\nFILES={{FILE_LIST}}\n"
                                    "TREE={{PROJECT_STRUCTURE}}\nCMDS={{SUPPORTED_COMMANDS}}\nPREV={{PREVIOUS_STEPS}}"));
    step.setAllowedCommands({QStringLiteral("OPEN_FILE")});
    const PromptResolver::ResolvedPrompt p = PromptResolver::resolve(step, session);

    QCOMPARE(p.userPrompt, QStringLiteral("explain main"));
    QVERIFY(p.systemPrompt.startsWith(QStringLiteral("Runtime date context:")));
    QVERIFY(p.systemPrompt.contains(QStringLiteral("Q=explain main")));
    QVERIFY(p.systemPrompt.contains(QStringLiteral("ROOT=") + session.project().root()));
    QVERIFY(p.systemPrompt.contains(QDir::toNativeSeparators(QDir(dir.path()).filePath(QStringLiteral("src/main.cpp")))));
    QVERIFY(p.systemPrompt.contains(QStringLiteral("  src/")));
    QVERIFY(p.systemPrompt.contains(QStringLiteral("OPEN_FILE")));
    QVERIFY(!p.systemPrompt.contains(QStringLiteral("CREATE_FILE")));  // only the step's commands
    QVERIFY(p.systemPrompt.contains(QStringLiteral("PREV=Nothing has been done yet.")));
}

void TestSession::promptResolverDropsUnknownPlaceholders()
{
    Session session;
    PromptStep step;
    step.setTemplate(QStringLiteral("a {{NO_SUCH_THING}} b"));
    QVERIFY(PromptResolver::resolve(step, session).systemPrompt.endsWith(QStringLiteral("a  b")));
}

void TestSession::projectListsFilesAndStructure()
{
    QTemporaryDir dir;
    QVERIFY(TestFiles::write(dir.filePath(QStringLiteral("a.txt")), "a"));
    QVERIFY(TestFiles::write(dir.filePath(QStringLiteral("lib/b.txt")), "b"));
    Project project;
    QSignalSpy scanned(&project, &Project::scanned);
    project.open(dir.path());
    QCOMPARE(scanned.count(), 1);

    QStringList files = project.files();
    files.sort();
    QCOMPARE(files, (QStringList{"a.txt", "lib/b.txt"}));  // the DevelopersAssistant folder is left out
    QCOMPARE(project.filesNeedingDescription().size(), 2);
    QVERIFY(project.structureText().contains(QStringLiteral("  lib/")));
}

void TestSession::workingContextKeepsNewestOutputs()
{
    WorkingContext context;
    for (int i = 1; i <= 7; ++i)
        context.addConsoleOutput(QStringLiteral("RUN_SHELL: step%1").arg(i), QStringLiteral("out %1").arg(i), QString(), 0);
    const QStringList logs = context.files().consoleLogs();
    QCOMPARE(logs.size(), 5);
    QVERIFY(logs.last().contains(QStringLiteral("out 7")));   // the newest is kept
    QVERIFY(!logs.first().contains(QStringLiteral("out 2")));

    // The same program again replaces its earlier output.
    context.addConsoleOutput(QStringLiteral("RUN_SHELL: step7"), QStringLiteral("again"), QString(), 0);
    QCOMPARE(context.files().consoleLogs().filter(QStringLiteral("RUN_SHELL: step7")).size(), 1);
}

void TestSession::workingContextForgetsChangedFiles()
{
    QTemporaryDir dir;
    QVERIFY(TestFiles::write(dir.filePath(QStringLiteral("a.txt")), "one\ntwo\n"));
    Session session;
    session.project().open(dir.path());
    WorkingContext& context = session.context();
    auto open = command(R"({"command":"OPEN_FILE","params":{"path":"a.txt"}})");
    context.open(open.get(), QString());
    QVERIFY(context.openFilesText().contains(QStringLiteral("2| two")));

    auto edit = command(R"({"command":"REPLACE_IN_FILE","params":{"path":"a.txt","content":"three"}})");
    context.forgetChangedFiles(edit.get());
    QVERIFY(!context.openFilesText().contains(QStringLiteral("two")));
}

QTEST_MAIN(TestSession)
#include "tst_session.moc"
