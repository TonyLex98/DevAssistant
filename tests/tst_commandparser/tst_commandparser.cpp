#include <QtTest>
#include <QJsonDocument>
#include <QSet>

#include "core/commands/CommandDocs.h"
#include "core/commands/CommandParser.h"
#include "core/commands/CommandRegistry.h"
#include "core/commands/EditInRangeCommand.h"

namespace {

// The commands parsed from one model reply; deleted with it.
struct Parsed
{
    explicit Parsed(const QByteArray& raw) : commands(CommandParser::parse(raw)) {}
    ~Parsed() { qDeleteAll(commands); }
    Parsed(const Parsed&) = delete;
    Parsed& operator=(const Parsed&) = delete;

    int size() const { return commands.size(); }
    Command* at(int i) const { return commands.at(i); }
    QJsonValue param(int i, const char* key) const { return commands.at(i)->params().value(QLatin1String(key)); }
    QStringList names() const
    {
        QStringList out;
        for (const Command* c : commands)
            out << c->name();
        return out;
    }

    QVector<Command*> commands;
};

QStringList registeredNames()
{
    QStringList names;
    for (Command* c : CommandRegistry::all()) {
        names << c->name();
        delete c;
    }
    return names;
}

} // namespace

class TestCommandParser : public QObject
{
    Q_OBJECT

private slots:
    void jsonArray();
    void singleObject();
    void concatenatedObjects();
    void proseFenceAndTrailingText();
    void truncatedArrayKeepsCompleteCommands();

    void variantsOfOneCommand_data();
    void variantsOfOneCommand();
    void dslCommandWithJsonParams();

    void dslWindowsPathKeptWhole();
    void dslSeveralLines();
    void proseGivesNoCommands_data();
    void proseGivesNoCommands();

    void numericStringsBecomeNumbers();
    void textInsideStringSurvivesRepair_data();
    void textInsideStringSurvivesRepair();
    void renameFileFromTo();
    void unknownCommandsSkipped();
    void toJsonRoundTrip();

    void registryNamesAreUnique();
    void registryCreatesEveryCommand_data();
    void registryCreatesEveryCommand();
    void docsExampleParsesBack_data();
    void docsExampleParsesBack();
};

void TestCommandParser::jsonArray()
{
    const Parsed p(R"([{"command":"OPEN_FILE","params":{"path":"src/main.cpp"}},)"
                   R"( {"command":"EDIT_IN_RANGE","params":{"path":"src/main.cpp","startLine":1,"endLine":2,"content":"x"}}])");
    QCOMPARE(p.names(), (QStringList{"OPEN_FILE", "EDIT_IN_RANGE"}));
    QCOMPARE(p.param(0, "path").toString(), QStringLiteral("src/main.cpp"));
    QCOMPARE(p.param(1, "endLine").toInt(), 2);
}

void TestCommandParser::singleObject()
{
    const Parsed p(R"({"command":"SEARCH_PROJECT","params":{"query":"TODO","maxMatches":5}})");
    QCOMPARE(p.names(), QStringList{"SEARCH_PROJECT"});
    QCOMPARE(p.param(0, "query").toString(), QStringLiteral("TODO"));
    QCOMPARE(p.param(0, "maxMatches").toInt(), 5);
}

void TestCommandParser::concatenatedObjects()
{
    const Parsed objects(R"({"command":"OPEN_FILE","params":{"path":"a.cpp"}})" "\n"
                         R"({"command":"OPEN_FILE","params":{"path":"b.cpp"}})");
    QCOMPARE(objects.size(), 2);
    QCOMPARE(objects.param(0, "path").toString(), QStringLiteral("a.cpp"));
    QCOMPARE(objects.param(1, "path").toString(), QStringLiteral("b.cpp"));

    const Parsed arrays(R"([{"command":"OPEN_FILE","params":{"path":"a.cpp"}}])"
                        R"([{"command":"SHOW_RESULT","params":{"content":"done"}}])");
    QCOMPARE(arrays.names(), (QStringList{"OPEN_FILE", "SHOW_RESULT"}));
}

void TestCommandParser::proseFenceAndTrailingText()
{
    const Parsed p("I will open the file first.\n"
                   "```json\n"
                   R"([{"command":"OPEN_FILE","params":{"path":"src/main.cpp"}}])" "\n"
                   "```\n"
                   "After that I will edit it.");
    QCOMPARE(p.names(), QStringList{"OPEN_FILE"});
    QCOMPARE(p.param(0, "path").toString(), QStringLiteral("src/main.cpp"));

    const Parsed fenceOnly("```json\n" R"({"command":"OPEN_FILE","params":{"path":"a.cpp"}})" "\n```");
    QCOMPARE(fenceOnly.names(), QStringList{"OPEN_FILE"});
}

void TestCommandParser::truncatedArrayKeepsCompleteCommands()
{
    // The closing bracket is missing.
    const Parsed noBracket(R"([{"command":"OPEN_FILE","params":{"path":"a.cpp"}},)"
                           R"({"command":"OPEN_FILE","params":{"path":"b.cpp"}})");
    QCOMPARE(noBracket.size(), 2);
    QCOMPARE(noBracket.param(1, "path").toString(), QStringLiteral("b.cpp"));

    // Cut off in the middle of the second command: the first one still runs.
    const Parsed cut(R"([{"command":"OPEN_FILE","params":{"path":"a.cpp"}},)"
                     R"({"command":"EDIT_IN_RANGE","params":{"path":"a.c)");
    QCOMPARE(cut.names(), QStringList{"OPEN_FILE"});
    QCOMPARE(cut.param(0, "path").toString(), QStringLiteral("a.cpp"));
}

void TestCommandParser::variantsOfOneCommand_data()
{
    QTest::addColumn<QByteArray>("raw");

    QTest::newRow("smart quotes") << QByteArray("[{“command”: “OPEN_FILE”, “params”: {“path”: “a.cpp”}}]");
    QTest::newRow("single quotes") << QByteArray("{'command': 'OPEN_FILE', 'params': {'path': 'a.cpp'}}");
    QTest::newRow("trailing commas") << QByteArray(R"([{"command":"OPEN_FILE","params":{"path":"a.cpp",},},])");
    QTest::newRow("bare keys") << QByteArray(R"({command: "OPEN_FILE", params: {path: "a.cpp"}})");
    QTest::newRow("name + arguments") << QByteArray(R"({"name":"OPEN_FILE","arguments":{"path":"a.cpp"}})");
    QTest::newRow("name + arguments as a string")
            << QByteArray(R"({"name":"read_file","arguments":"{\"path\":\"a.cpp\"}"})");
    QTest::newRow("tool + args, alias key") << QByteArray(R"({"tool":"open_file","args":{"file_path":"a.cpp"}})");
    QTest::newRow("action, top-level params") << QByteArray(R"({"action":"OPEN_FILE","file_path":"a.cpp"})");
    QTest::newRow("DSL key=value") << QByteArray("OPEN_FILE path=a.cpp");
    QTest::newRow("DSL quoted value") << QByteArray("OPEN_FILE path=\"a.cpp\"");
    QTest::newRow("DSL bare path") << QByteArray("OPEN_FILE a.cpp");
    QTest::newRow("DSL key: value") << QByteArray("OPEN_FILE path: a.cpp");
    QTest::newRow("DSL list item") << QByteArray("1. OPEN_FILE path=a.cpp");
    QTest::newRow("DSL bullet") << QByteArray("- OPEN_FILE path=a.cpp");
}

void TestCommandParser::variantsOfOneCommand()
{
    QFETCH(QByteArray, raw);
    const Parsed p(raw);
    QCOMPARE(p.names(), QStringList{"OPEN_FILE"});
    QCOMPARE(p.param(0, "path").toString(), QStringLiteral("a.cpp"));
}

void TestCommandParser::dslCommandWithJsonParams()
{
    const Parsed p(R"(OPEN_FILE {"path":"a.cpp"})");
    QCOMPARE(p.names(), QStringList{"OPEN_FILE"});
    QCOMPARE(p.param(0, "path").toString(), QStringLiteral("a.cpp"));
}

void TestCommandParser::dslWindowsPathKeptWhole()
{
    const Parsed bare(R"(OPEN_FILE C:\src\a.cpp)");
    QCOMPARE(bare.names(), QStringList{"OPEN_FILE"});
    QCOMPARE(bare.param(0, "path").toString(), QStringLiteral("C:\\src\\a.cpp"));

    const Parsed keyed(R"(OPEN_FILE path=C:\src\a.cpp)");
    QCOMPARE(keyed.param(0, "path").toString(), QStringLiteral("C:\\src\\a.cpp"));
}

void TestCommandParser::dslSeveralLines()
{
    const Parsed p("OPEN_FILE path=src/main.cpp\n"
                   "SEARCH_PROJECT query=\"class Foo\" maxMatches=3\n");
    QCOMPARE(p.names(), (QStringList{"OPEN_FILE", "SEARCH_PROJECT"}));
    QCOMPARE(p.param(0, "path").toString(), QStringLiteral("src/main.cpp"));
    QCOMPARE(p.param(1, "query").toString(), QStringLiteral("class Foo"));
    QCOMPARE(p.param(1, "maxMatches").toInt(), 3);
}

void TestCommandParser::proseGivesNoCommands_data()
{
    QTest::addColumn<QByteArray>("raw");
    QTest::newRow("numbered step") << QByteArray("2. Undo the previous change");
    QTest::newRow("sentence") << QByteArray("Open the file");
    QTest::newRow("sentence with a path") << QByteArray("Open the file src/main.cpp and fix the bug.");
    QTest::newRow("upper-case word that is not a command") << QByteArray("UNDO the change");
    QTest::newRow("plan") << QByteArray("Plan:\n1. Open the file\n2. Edit line 3\n3. Show the result");
    QTest::newRow("empty") << QByteArray();
}

void TestCommandParser::proseGivesNoCommands()
{
    QFETCH(QByteArray, raw);
    const Parsed p(raw);
    QCOMPARE(p.names(), QStringList());
}

void TestCommandParser::numericStringsBecomeNumbers()
{
    const Parsed edit(R"({"command":"EDIT_IN_RANGE","params":{"path":"a.cpp","startLine":"2","endLine":" 3 ","content":"x"}})");
    QCOMPARE(edit.size(), 1);
    auto* cmd = dynamic_cast<EditInRangeCommand*>(edit.at(0));
    QVERIFY(cmd);
    QCOMPARE(cmd->startLine(), 2);
    QCOMPARE(cmd->endLine(), 3);

    const Parsed insert(R"({"command":"INSERT_AT_LINE","params":{"path":"a.cpp","line":"4","content":"x"}})");
    QCOMPARE(insert.param(0, "line").toInt(), 4);

    const Parsed del(R"({"command":"DELETE_DIR","params":{"path":"tmp","recursive":"true"}})");
    QVERIFY(del.param(0, "recursive").toBool());

    const Parsed search(R"({"command":"SEARCH_PROJECT","params":{"query":"x","maxMatches":"7","caseSensitive":"yes"}})");
    QCOMPARE(search.param(0, "maxMatches").toInt(), 7);
    QVERIFY(search.param(0, "caseSensitive").toBool());
}

void TestCommandParser::textInsideStringSurvivesRepair_data()
{
    QTest::addColumn<QByteArray>("raw");
    QTest::newRow("bare keys") << QByteArray(R"({command: "SHOW_RESULT", params: {title: "Done", content: "Result: [ok]"}})");
    QTest::newRow("trailing comma") << QByteArray(R"({"command":"SHOW_RESULT","params":{"content":"Result: [ok]",}})");
    QTest::newRow("single quotes") << QByteArray("{'command': 'SHOW_RESULT', 'params': {'content': 'Result: [ok]'}}");
}

void TestCommandParser::textInsideStringSurvivesRepair()
{
    QFETCH(QByteArray, raw);
    const Parsed p(raw);
    QCOMPARE(p.names(), QStringList{"SHOW_RESULT"});
    QCOMPARE(p.param(0, "content").toString(), QStringLiteral("Result: [ok]"));
}

void TestCommandParser::renameFileFromTo()
{
    const Parsed p(R"({"command":"RENAME_FILE","params":{"from":"a.txt","to":"dir/b.txt"}})");
    QCOMPARE(p.names(), QStringList{"RENAME_FILE"});
    QCOMPARE(p.param(0, "oldPath").toString(), QStringLiteral("a.txt"));
    QCOMPARE(p.param(0, "newPath").toString(), QStringLiteral("dir/b.txt"));

    const Parsed alias(R"({"command":"move_file","params":{"old_path":"a.txt","new_path":"b.txt"}})");
    QCOMPARE(alias.names(), QStringList{"RENAME_FILE"});
    QCOMPARE(alias.param(0, "oldPath").toString(), QStringLiteral("a.txt"));
    QCOMPARE(alias.param(0, "newPath").toString(), QStringLiteral("b.txt"));
}

void TestCommandParser::unknownCommandsSkipped()
{
    const Parsed p(R"([{"command":"FORMAT_DISK","params":{}},{"command":"OPEN_FILE","params":{"path":"a.cpp"}},{"params":{}}])");
    QCOMPARE(p.names(), QStringList{"OPEN_FILE"});

    QScopedPointer<Command> unknown(CommandRegistry::create(QJsonObject{{"command", "FORMAT_DISK"}}));
    QVERIFY(!unknown);
    QScopedPointer<Command> nameless(CommandRegistry::create(QJsonObject()));
    QVERIFY(!nameless);
}

void TestCommandParser::toJsonRoundTrip()
{
    const Parsed first(R"([{"command":"OPEN_FILE","params":{"path":"a.cpp"}},)"
                       R"({"command":"EDIT_IN_RANGE","params":{"path":"a.cpp","startLine":2,"endLine":3,"content":"x\ny"}},)"
                       R"({"command":"RUN_SHELL","params":{"command":"git status","cwd":"src"}}])");
    QCOMPARE(first.size(), 3);

    const Parsed second(CommandParser::toJson(first.commands));
    QCOMPARE(second.names(), first.names());
    for (int i = 0; i < first.size(); ++i)
        QCOMPARE(second.at(i)->params().value("path"), first.at(i)->params().value("path"));
    QCOMPARE(second.param(1, "content").toString(), QStringLiteral("x\ny"));
    QCOMPARE(second.param(2, "command").toString(), QStringLiteral("git status"));
}

void TestCommandParser::registryNamesAreUnique()
{
    const QStringList names = registeredNames();
    QVERIFY2(names.size() >= 30, qPrintable(QString::number(names.size())));
    const QSet<QString> unique(names.begin(), names.end());
    QCOMPARE(unique.size(), names.size());
    for (const QString& n : names)
        QVERIFY2(!n.isEmpty() && n == n.toUpper(), qPrintable(n));
}

void TestCommandParser::registryCreatesEveryCommand_data()
{
    QTest::addColumn<QString>("name");
    for (const QString& n : registeredNames())
        QTest::newRow(qPrintable(n)) << n;
}

void TestCommandParser::registryCreatesEveryCommand()
{
    QFETCH(QString, name);

    QScopedPointer<Command> exact(CommandRegistry::create(QJsonObject{{"command", name}}));
    QVERIFY(exact);
    QCOMPARE(exact->name(), name);

    QScopedPointer<Command> lower(CommandRegistry::create(QJsonObject{{"command", name.toLower()}}));
    QVERIFY(lower);
    QCOMPARE(lower->name(), name);
}

void TestCommandParser::docsExampleParsesBack_data()
{
    QTest::addColumn<QString>("name");
    for (const QString& n : registeredNames())
        QTest::newRow(qPrintable(n)) << n;
}

void TestCommandParser::docsExampleParsesBack()
{
    QFETCH(QString, name);

    const CommandDoc doc = CommandDocs::doc(name);
    QVERIFY2(doc.summary != CommandDocs::doc(QStringLiteral("NO_SUCH_COMMAND")).summary,
             "the command has no documentation of its own");
    QCOMPARE(doc.example.value("command").toString(), name);

    const QByteArray json = QJsonDocument(doc.example).toJson(QJsonDocument::Compact);
    const Parsed p(json);
    QVERIFY2(p.size() == 1, json.constData());
    QCOMPARE(p.at(0)->name(), name);

    QVERIFY(CommandDocs::renderForLLM(doc).contains(name));
}

QTEST_MAIN(TestCommandParser)
#include "tst_commandparser.moc"
