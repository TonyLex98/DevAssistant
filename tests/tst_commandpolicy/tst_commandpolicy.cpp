#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include "Command.h"
#include "CommandPolicy.h"

using CommandPolicy::Risk;

namespace {

// A command with a given name and parameters; the policy only looks at those two.
class FakeCommand : public Command
{
public:
    explicit FakeCommand(const QString& name, const QJsonObject& params = QJsonObject())
        : m_name(name), m_params(params) {}

    QString name() const override { return m_name; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return QStringLiteral("fake"); }
    bool apply(DevAssManager*, QString*) override { return true; }
    QJsonObject toJson() const override { return QJsonObject{{"command", m_name}, {"params", m_params}}; }

private:
    QString m_name;
    QJsonObject m_params;
};

} // namespace

class TestCommandPolicy : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void riskOf_data();
    void riskOf();
    void riskLabelsDiffer();
    void isReadOnlyBatch();

    void checkRejectsPathOutsideProject_data();
    void checkRejectsPathOutsideProject();
    void checkAcceptsPathsInsideProject();
    void checkRejectsModifyingCommandNotInAllowedList();
    void checkAllowsReadOnlyCommandNotInAllowedList();
    void checkAllowedListIgnoresCaseAndSpaces();
    void checkWithoutAllowedListAllowsEverything();
    void checkWithoutProjectRejectsPaths();
    void checkCommandObject();

    void detailsRunShellShowsCommandLine();
    void detailsEditInRangeShowsPathAndLines();
    void detailsOtherCommands();

private:
    QTemporaryDir m_tmp;
    QString m_root;
};

void TestCommandPolicy::initTestCase()
{
    QVERIFY(m_tmp.isValid());
    m_root = m_tmp.path() + QStringLiteral("/project");
    QVERIFY(QDir().mkpath(m_root + QStringLiteral("/src")));
}

void TestCommandPolicy::riskOf_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("risk");

    const int readOnly = int(Risk::ReadOnly);
    const int modifying = int(Risk::Modifying);
    const int dangerous = int(Risk::Dangerous);

    QTest::newRow("OPEN_FILE") << QStringLiteral("OPEN_FILE") << readOnly;
    QTest::newRow("open_file, lower case") << QStringLiteral("open_file") << readOnly;
    QTest::newRow("OPEN_FILE_REGION, padded") << QStringLiteral(" OPEN_FILE_REGION ") << readOnly;
    QTest::newRow("SEARCH_PROJECT") << QStringLiteral("SEARCH_PROJECT") << readOnly;
    QTest::newRow("SHOW_RESULT") << QStringLiteral("SHOW_RESULT") << readOnly;
    QTest::newRow("GIT_DIFF") << QStringLiteral("GIT_DIFF") << readOnly;
    QTest::newRow("EDIT_IN_RANGE") << QStringLiteral("EDIT_IN_RANGE") << modifying;
    QTest::newRow("CREATE_FILE") << QStringLiteral("CREATE_FILE") << modifying;
    QTest::newRow("APPLY_PATCH") << QStringLiteral("APPLY_PATCH") << modifying;
    QTest::newRow("RENAME_FILE") << QStringLiteral("RENAME_FILE") << modifying;
    QTest::newRow("UNDO_LAST_CHANGE") << QStringLiteral("UNDO_LAST_CHANGE") << modifying;
    QTest::newRow("RUN_SHELL") << QStringLiteral("RUN_SHELL") << dangerous;
    QTest::newRow("RUN_PYTHON") << QStringLiteral("RUN_PYTHON") << dangerous;
    QTest::newRow("DELETE_DIR") << QStringLiteral("DELETE_DIR") << dangerous;
    QTest::newRow("DELETE_FILE") << QStringLiteral("DELETE_FILE") << dangerous;
    QTest::newRow("OPEN_URL") << QStringLiteral("OPEN_URL") << dangerous;
    QTest::newRow("GIT_RESTORE_FILE") << QStringLiteral("GIT_RESTORE_FILE") << dangerous;
    QTest::newRow("unknown") << QStringLiteral("FORMAT_DISK") << dangerous;
    QTest::newRow("empty") << QString() << dangerous;
}

void TestCommandPolicy::riskOf()
{
    QFETCH(QString, name);
    QFETCH(int, risk);
    QCOMPARE(int(CommandPolicy::riskOf(name)), risk);
}

void TestCommandPolicy::riskLabelsDiffer()
{
    const QString r = CommandPolicy::riskLabel(Risk::ReadOnly);
    const QString m = CommandPolicy::riskLabel(Risk::Modifying);
    const QString d = CommandPolicy::riskLabel(Risk::Dangerous);
    QVERIFY(!r.isEmpty() && !m.isEmpty() && !d.isEmpty());
    QVERIFY(r != m && m != d && r != d);
}

void TestCommandPolicy::isReadOnlyBatch()
{
    QVERIFY2(!CommandPolicy::isReadOnlyBatch({}), "an empty batch is not read-only");

    FakeCommand open(QStringLiteral("OPEN_FILE"));
    FakeCommand search(QStringLiteral("SEARCH_PROJECT"));
    FakeCommand edit(QStringLiteral("EDIT_IN_RANGE"));
    FakeCommand shell(QStringLiteral("RUN_SHELL"));

    QVERIFY(CommandPolicy::isReadOnlyBatch({&open}));
    QVERIFY(CommandPolicy::isReadOnlyBatch({&open, &search}));
    QVERIFY(!CommandPolicy::isReadOnlyBatch({&open, &edit}));
    QVERIFY(!CommandPolicy::isReadOnlyBatch({&shell}));
    QVERIFY(!CommandPolicy::isReadOnlyBatch({&open, nullptr}));
}

void TestCommandPolicy::checkRejectsPathOutsideProject_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QJsonObject>("params");
    QTest::addColumn<QString>("key");

    QTest::newRow("../ in path") << QStringLiteral("OPEN_FILE")
                                 << QJsonObject{{"path", "../secret.txt"}} << QStringLiteral("path");
    QTest::newRow("deep ../ in path") << QStringLiteral("EDIT_IN_RANGE")
                                      << QJsonObject{{"path", "src/../../secret.txt"}, {"startLine", 1}, {"endLine", 1}}
                                      << QStringLiteral("path");
    QTest::newRow("absolute path outside") << QStringLiteral("CREATE_FILE")
                                           << QJsonObject{{"path", QDir::tempPath() + "/devass-outside.txt"}}
                                           << QStringLiteral("path");
    QTest::newRow("rename target") << QStringLiteral("RENAME_FILE")
                                   << QJsonObject{{"oldPath", "src/a.cpp"}, {"newPath", "../a.cpp"}}
                                   << QStringLiteral("newPath");
    QTest::newRow("copy source") << QStringLiteral("COPY_FILE")
                                 << QJsonObject{{"from", "../../etc/passwd"}, {"to", "copy.txt"}}
                                 << QStringLiteral("from");
    QTest::newRow("shell cwd") << QStringLiteral("RUN_SHELL")
                               << QJsonObject{{"command", "dir"}, {"cwd", ".."}} << QStringLiteral("cwd");
    QTest::newRow("list folder") << QStringLiteral("LIST_FILES_IN_FOLDER")
                                 << QJsonObject{{"folderPath", "../.."}} << QStringLiteral("folderPath");
}

void TestCommandPolicy::checkRejectsPathOutsideProject()
{
    QFETCH(QString, name);
    QFETCH(QJsonObject, params);
    QFETCH(QString, key);

    QString err;
    QVERIFY(!CommandPolicy::check(name, params, m_root, QStringList(), &err));
    QVERIFY2(err.contains(key), qPrintable(err));
    QVERIFY2(err.contains(name), qPrintable(err));
}

void TestCommandPolicy::checkAcceptsPathsInsideProject()
{
    QString err;
    QVERIFY2(CommandPolicy::check(QStringLiteral("EDIT_IN_RANGE"),
                                  QJsonObject{{"path", "src/a.cpp"}, {"startLine", 1}, {"endLine", 2}},
                                  m_root, QStringList(), &err), qPrintable(err));
    QVERIFY2(CommandPolicy::check(QStringLiteral("RENAME_FILE"),
                                  QJsonObject{{"oldPath", "src/a.cpp"}, {"newPath", m_root + "/src/b.cpp"}},
                                  m_root, QStringList(), &err), qPrintable(err));
    // Empty and non-string path values are not paths.
    QVERIFY2(CommandPolicy::check(QStringLiteral("SEARCH_PROJECT"),
                                  QJsonObject{{"query", "x"}, {"path", ""}, {"cwd", 5}},
                                  m_root, QStringList(), &err), qPrintable(err));
}

void TestCommandPolicy::checkRejectsModifyingCommandNotInAllowedList()
{
    const QStringList allowed{QStringLiteral("OPEN_FILE"), QStringLiteral("CREATE_FILE")};
    QString err;
    QVERIFY(!CommandPolicy::check(QStringLiteral("EDIT_IN_RANGE"), QJsonObject{{"path", "src/a.cpp"}},
                                  m_root, allowed, &err));
    QVERIFY2(err.contains(QStringLiteral("EDIT_IN_RANGE")), qPrintable(err));

    err.clear();
    QVERIFY(!CommandPolicy::check(QStringLiteral("RUN_SHELL"), QJsonObject{{"command", "dir"}},
                                  m_root, allowed, &err));
    QVERIFY(!err.isEmpty());

    QVERIFY(CommandPolicy::check(QStringLiteral("CREATE_FILE"), QJsonObject{{"path", "src/new.cpp"}},
                                 m_root, allowed, &err));
}

void TestCommandPolicy::checkAllowsReadOnlyCommandNotInAllowedList()
{
    const QStringList allowed{QStringLiteral("CREATE_FILE")};
    QString err;
    QVERIFY2(CommandPolicy::check(QStringLiteral("OPEN_FILE"), QJsonObject{{"path", "src/a.cpp"}},
                                  m_root, allowed, &err), qPrintable(err));
    QVERIFY2(CommandPolicy::check(QStringLiteral("SHOW_RESULT"), QJsonObject{{"content", "done"}},
                                  m_root, allowed, &err), qPrintable(err));
    // ...but its paths are still checked.
    QVERIFY(!CommandPolicy::check(QStringLiteral("OPEN_FILE"), QJsonObject{{"path", "../x.txt"}},
                                  m_root, allowed, &err));
}

void TestCommandPolicy::checkAllowedListIgnoresCaseAndSpaces()
{
    QString err;
    QVERIFY2(CommandPolicy::check(QStringLiteral("edit_in_range"), QJsonObject{{"path", "src/a.cpp"}},
                                  m_root, QStringList{QStringLiteral(" EDIT_IN_RANGE ")}, &err), qPrintable(err));
    QVERIFY2(CommandPolicy::check(QStringLiteral("EDIT_IN_RANGE"), QJsonObject{{"path", "src/a.cpp"}},
                                  m_root, QStringList{QStringLiteral("edit_in_range")}, &err), qPrintable(err));
}

void TestCommandPolicy::checkWithoutAllowedListAllowsEverything()
{
    QString err;
    QVERIFY2(CommandPolicy::check(QStringLiteral("RUN_SHELL"), QJsonObject{{"command", "dir"}, {"cwd", "src"}},
                                  m_root, QStringList(), &err), qPrintable(err));
    QVERIFY2(CommandPolicy::check(QStringLiteral("DELETE_DIR"), QJsonObject{{"path", "src"}},
                                  m_root, QStringList(), &err), qPrintable(err));
}

void TestCommandPolicy::checkWithoutProjectRejectsPaths()
{
    QString err;
    QVERIFY(!CommandPolicy::check(QStringLiteral("OPEN_FILE"), QJsonObject{{"path", "a.cpp"}},
                                  QString(), QStringList(), &err));
    QVERIFY(!err.isEmpty());
    // A command without paths does not need a project.
    QVERIFY2(CommandPolicy::check(QStringLiteral("SHOW_RESULT"), QJsonObject{{"content", "x"}},
                                  QString(), QStringList(), &err), qPrintable(err));
}

void TestCommandPolicy::checkCommandObject()
{
    QString err;
    const FakeCommand inside(QStringLiteral("EDIT_IN_RANGE"), QJsonObject{{"path", "src/a.cpp"}});
    QVERIFY2(CommandPolicy::check(&inside, m_root, QStringList(), &err), qPrintable(err));

    const FakeCommand outside(QStringLiteral("EDIT_IN_RANGE"), QJsonObject{{"path", "../a.cpp"}});
    QVERIFY(!CommandPolicy::check(&outside, m_root, QStringList(), &err));

    err.clear();
    QVERIFY(!CommandPolicy::check(nullptr, m_root, QStringList(), &err));
    QVERIFY(!err.isEmpty());
}

void TestCommandPolicy::detailsRunShellShowsCommandLine()
{
    const QString d = CommandPolicy::details(QStringLiteral("RUN_SHELL"),
            QJsonObject{{"command", "git status --short"}, {"cwd", "src"},
                        {"description", "just looking"}}, m_root);
    QVERIFY2(d.startsWith(QStringLiteral("> git status --short")), qPrintable(d));
    QVERIFY2(d.contains(QStringLiteral("src")), qPrintable(d));
    QVERIFY2(!d.contains(QStringLiteral("just looking")), "details() must not use the model's description");
}

void TestCommandPolicy::detailsEditInRangeShowsPathAndLines()
{
    const QString expected = QDir::toNativeSeparators(QStringLiteral("src/a.cpp")) + QStringLiteral(", lines 3–5");

    QString d = CommandPolicy::details(QStringLiteral("EDIT_IN_RANGE"),
            QJsonObject{{"path", "src/a.cpp"}, {"startLine", 3}, {"endLine", 5}, {"content", "x\ny"}}, m_root);
    QVERIFY2(d.startsWith(expected), qPrintable(d));
    QVERIFY2(d.contains(QStringLiteral("2 line")), qPrintable(d));

    // Numbers given as strings, and an absolute path inside the project shown relative to it.
    d = CommandPolicy::details(QStringLiteral("EDIT_IN_RANGE"),
            QJsonObject{{"path", m_root + "/src/a.cpp"}, {"startLine", "3"}, {"endLine", "5"}}, m_root);
    QVERIFY2(d.startsWith(expected), qPrintable(d));
}

void TestCommandPolicy::detailsOtherCommands()
{
    QCOMPARE(CommandPolicy::details(QStringLiteral("OPEN_URL"),
                                    QJsonObject{{"url", "https://example.com"}, {"description", "docs"}}, m_root),
             QStringLiteral("https://example.com"));

    const QString del = CommandPolicy::details(QStringLiteral("DELETE_DIR"),
                                               QJsonObject{{"path", "build"}, {"recursive", true}}, m_root);
    QVERIFY2(del.startsWith(QStringLiteral("build")), qPrintable(del));
    QVERIFY2(del != CommandPolicy::details(QStringLiteral("DELETE_DIR"),
                                           QJsonObject{{"path", "build"}, {"recursive", false}}, m_root),
             "a recursive delete must read differently");

    const QString rename = CommandPolicy::details(QStringLiteral("RENAME_FILE"),
            QJsonObject{{"oldPath", "a.txt"}, {"newPath", "b.txt"}}, m_root);
    QCOMPARE(rename, QStringLiteral("a.txt → b.txt"));

    // A path outside the project is shown in full.
    const QString outside = m_tmp.path() + QStringLiteral("/other.txt");
    QCOMPARE(CommandPolicy::details(QStringLiteral("OPEN_FILE"), QJsonObject{{"path", outside}}, m_root),
             QDir::toNativeSeparators(outside));

    const QString search = CommandPolicy::details(QStringLiteral("SEARCH_PROJECT"),
                                                  QJsonObject{{"query", "TODO"}}, m_root);
    QCOMPARE(search, QStringLiteral("\"TODO\""));
}

QTEST_MAIN(TestCommandPolicy)
#include "tst_commandpolicy.moc"
