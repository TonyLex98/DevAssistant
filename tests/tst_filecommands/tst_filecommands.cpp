#include <QtTest>
#include <QDir>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "DevAssManager.h"
#include "AppendToFileCommand.h"
#include "ApplyPatchCommand.h"
#include "CreateFileCommand.h"
#include "DeleteDirCommand.h"
#include "DeleteFileCommand.h"
#include "EditInRangeCommand.h"
#include "InsertAtLineCommand.h"
#include "OpenFileRegionCommand.h"
#include "RenameFileCommand.h"
#include "SearchProjectCommand.h"
#include "UndoLastChangeCommand.h"
#include "TestFiles.h"

// Commands run against a temporary project through DevAssManager, as the app runs them.
class TestFileCommands : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void editReplacesLines();
    void editContentWithFinalNewlineAddsNoBlankLine();
    void editWithEmptyContentDeletesLines();
    void editWithReplacementLines();
    void editOutOfRangeFails_data();
    void editOutOfRangeFails();
    void editBatchUsesOriginalNumbering();
    void editKeepsCrlf();
    void editKeepsUtf8();
    void editKeepsBom();
    void editKeepsMissingFinalNewline();

    void insertAtSizePlusOneAppends();
    void insertAtSizePlusTwoFails();
    void insertAtFirstLine();
    void insertIntoEmptyFile();

    void appendEnsureNewline();
    void appendCreatesMissingFile();

    void createOnExistingFileFails();
    void createThenUndoRemovesFile();
    void deleteThenUndoRestoresBytes();
    void renameOntoExistingFileFails();
    void renameThenUndo();
    void undoRevertsInReverseOrder();
    void undoWithoutHistoryFails();

    void applyPatchSingleHunk();
    void applyPatchCreatesFileFromDevNull();
    void applyPatchContextMismatchLeavesFileUntouched();
    void applyPatchLinesThatLookLikeHeaders();
    void applyPatchHeaderWithTimestamp();
    void applyPatchStripOne();
    void applyPatchTwoHunks();
    void applyPatchKeepsCrlf();
    void applyPatchFailureInSecondFileChangesNothing();

    void openFileRegionLines();
    void openFileRegionRegex();
    void openFileRegionInvalidRegexFails();

    void searchIsCaseInsensitiveByDefault();
    void searchMaxMatches();
    void searchRelativeExcludePaths();
    void searchRelativeIncludePaths();

    void deleteDirDotDotRefused();
    void deleteDirProjectRootRefused();
    void deleteDirInsideProject();
    void deleteEmptyDirThenUndo();

    void changesOutsideProjectRefused();
    void commandsOutsideProjectRefused();
    void noProjectRefusesChanges();

private:
    QString abs(const QString& rel) const { return m_root + QLatin1Char('/') + rel; }
    bool write(const QString& rel, const QByteArray& data) const { return TestFiles::write(abs(rel), data); }
    QByteArray read(const QString& rel) const { return TestFiles::read(abs(rel)); }
    bool exists(const QString& rel) const { return QFileInfo::exists(abs(rel)); }
    bool undo(QString* err = nullptr)
    {
        UndoLastChangeCommand cmd;
        return cmd.apply(m_dam.data(), err);
    }

    QScopedPointer<QTemporaryDir> m_tmp;
    QString m_root;  // <tmp>/project, so that <tmp> is a place outside the project
    QScopedPointer<DevAssManager> m_dam;
};

void TestFileCommands::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestFileCommands::init()
{
    m_tmp.reset(new QTemporaryDir);
    QVERIFY(m_tmp->isValid());
    m_root = m_tmp->path() + QStringLiteral("/project");
    QVERIFY(QDir().mkpath(m_root));
    m_dam.reset(new DevAssManager(m_root));
}

void TestFileCommands::cleanup()
{
    m_dam.reset();
    m_tmp.reset();
}

/* ---- EDIT_IN_RANGE ---- */

void TestFileCommands::editReplacesLines()
{
    QVERIFY(write("f.txt", "a\nb\nc\nd\n"));
    EditInRangeCommand cmd("f.txt", 2, 3, "X\nY");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("a\nX\nY\nd\n"));
}

void TestFileCommands::editContentWithFinalNewlineAddsNoBlankLine()
{
    QVERIFY(write("f.txt", "a\nb\nc\nd\n"));
    EditInRangeCommand cmd("f.txt", 2, 2, "X\n");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("a\nX\nc\nd\n"));
}

void TestFileCommands::editWithEmptyContentDeletesLines()
{
    QVERIFY(write("f.txt", "a\nb\nc\nd\n"));
    EditInRangeCommand cmd("f.txt", 2, 3, QByteArray());
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("a\nd\n"));
}

void TestFileCommands::editWithReplacementLines()
{
    QVERIFY(write("f.txt", "a\nb\nc\n"));
    const QJsonArray lines{QJsonObject{{"line", 2}, {"text", "B1"}}, QJsonObject{{"line", 3}, {"text", "B2"}}};
    EditInRangeCommand cmd("f.txt", 2, 2, QByteArray(), lines);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("a\nB1\nB2\nc\n"));
}

void TestFileCommands::editOutOfRangeFails_data()
{
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("end");
    QTest::newRow("end past the last line") << 4 << 5;
    QTest::newRow("start past the last line") << 5 << 5;
    QTest::newRow("line zero") << 0 << 1;
    QTest::newRow("end before start") << 3 << 2;
}

void TestFileCommands::editOutOfRangeFails()
{
    QFETCH(int, start);
    QFETCH(int, end);
    QVERIFY(write("f.txt", "a\nb\nc\nd\n"));
    EditInRangeCommand cmd("f.txt", start, end, "X");
    QString err;
    QVERIFY(!cmd.apply(m_dam.data(), &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(read("f.txt"), QByteArray("a\nb\nc\nd\n"));
}

void TestFileCommands::editBatchUsesOriginalNumbering()
{
    QVERIFY(write("f.txt", "1\n2\n3\n4\n5\n"));
    EditInRangeCommand first("f.txt", 2, 2, "two-a\ntwo-b");
    EditInRangeCommand second("f.txt", 4, 4, "four");
    QString err;
    // Given in either order, both edits use the line numbers of the file as it was.
    QVERIFY2(EditInRangeCommand::applyBatch({&first, &second}, m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("1\ntwo-a\ntwo-b\n3\nfour\n5\n"));

    // The batch is one undo step.
    QVERIFY2(undo(&err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("1\n2\n3\n4\n5\n"));

    EditInRangeCommand third("f.txt", 5, 5, "five");
    EditInRangeCommand fourth("f.txt", 1, 1, "one");
    QVERIFY2(EditInRangeCommand::applyBatch({&third, &fourth}, m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("one\n2\n3\n4\nfive\n"));
}

void TestFileCommands::editKeepsCrlf()
{
    QVERIFY(write("f.txt", "a\r\nb\r\nc\r\n"));
    EditInRangeCommand cmd("f.txt", 2, 2, "X\nY\n");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("a\r\nX\r\nY\r\nc\r\n"));
}

void TestFileCommands::editKeepsUtf8()
{
    QVERIFY(write("f.txt", QStringLiteral("Привет\nмир\n").toUtf8()));
    EditInRangeCommand cmd("f.txt", 2, 2, QStringLiteral("Мир, ё!").toUtf8());
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(QString::fromUtf8(read("f.txt")), QStringLiteral("Привет\nМир, ё!\n"));
}

void TestFileCommands::editKeepsBom()
{
    QVERIFY(write("f.txt", QByteArray("\xEF\xBB\xBF" "a\nb\n")));
    EditInRangeCommand cmd("f.txt", 2, 2, "B");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("\xEF\xBB\xBF" "a\nB\n"));
}

void TestFileCommands::editKeepsMissingFinalNewline()
{
    QVERIFY(write("f.txt", "a\nb"));
    EditInRangeCommand cmd("f.txt", 1, 1, "A");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("A\nb"));
}

/* ---- INSERT_AT_LINE ---- */

void TestFileCommands::insertAtSizePlusOneAppends()
{
    QVERIFY(write("f.txt", "a\nb\n"));
    InsertAtLineCommand cmd("f.txt", 3, "c");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("a\nb\nc\n"));
}

void TestFileCommands::insertAtSizePlusTwoFails()
{
    QVERIFY(write("f.txt", "a\nb\n"));
    InsertAtLineCommand cmd("f.txt", 4, "c");
    QString err;
    QVERIFY(!cmd.apply(m_dam.data(), &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(read("f.txt"), QByteArray("a\nb\n"));
}

void TestFileCommands::insertAtFirstLine()
{
    QVERIFY(write("f.txt", "a\r\nb\r\n"));
    InsertAtLineCommand cmd("f.txt", 1, "x\ny\n");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("x\r\ny\r\na\r\nb\r\n"));
}

void TestFileCommands::insertIntoEmptyFile()
{
    QVERIFY(write("f.txt", QByteArray()));
    InsertAtLineCommand cmd("f.txt", 1, "first");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("first\n"));
}

/* ---- APPEND_TO_FILE ---- */

void TestFileCommands::appendEnsureNewline()
{
    QVERIFY(write("with.txt", "a"));
    QVERIFY(write("without.txt", "a"));
    QVERIFY(write("ends.txt", "a\n"));
    QString err;

    AppendToFileCommand with("with.txt", "b", true, true);
    QVERIFY2(with.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("with.txt"), QByteArray("a\nb"));

    AppendToFileCommand without("without.txt", "b", true, false);
    QVERIFY2(without.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("without.txt"), QByteArray("ab"));

    AppendToFileCommand ends("ends.txt", "b", true, true);
    QVERIFY2(ends.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("ends.txt"), QByteArray("a\nb"));
}

void TestFileCommands::appendCreatesMissingFile()
{
    QString err;
    AppendToFileCommand create("new/log.txt", "line\n", true, true);
    QVERIFY2(create.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("new/log.txt"), QByteArray("line\n"));

    AppendToFileCommand noCreate("missing.txt", "x", false, false);
    QVERIFY(!noCreate.apply(m_dam.data(), &err));
    QVERIFY(!exists("missing.txt"));
}

/* ---- CREATE / DELETE / RENAME / UNDO ---- */

void TestFileCommands::createOnExistingFileFails()
{
    QVERIFY(write("f.txt", "old"));
    CreateFileCommand cmd("f.txt", "new");
    QString err;
    QVERIFY(!cmd.apply(m_dam.data(), &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(read("f.txt"), QByteArray("old"));
}

void TestFileCommands::createThenUndoRemovesFile()
{
    CreateFileCommand cmd("dir/new.txt", "hello\n");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("dir/new.txt"), QByteArray("hello\n"));

    QVERIFY2(undo(&err), qPrintable(err));
    QVERIFY(!exists("dir/new.txt"));
}

void TestFileCommands::deleteThenUndoRestoresBytes()
{
    const QByteArray bytes("a\r\nb\0c\xff\n", 8);
    QVERIFY(write("bin.dat", bytes));
    DeleteFileCommand cmd("bin.dat");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QVERIFY(!exists("bin.dat"));

    QVERIFY2(undo(&err), qPrintable(err));
    QCOMPARE(read("bin.dat"), bytes);

    DeleteFileCommand missing("missing.txt");
    QVERIFY(!missing.apply(m_dam.data(), &err));
}

void TestFileCommands::renameOntoExistingFileFails()
{
    QVERIFY(write("a.txt", "A"));
    QVERIFY(write("b.txt", "B"));
    RenameFileCommand cmd("a.txt", "b.txt");
    QString err;
    QVERIFY(!cmd.apply(m_dam.data(), &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(read("a.txt"), QByteArray("A"));
    QCOMPARE(read("b.txt"), QByteArray("B"));
}

void TestFileCommands::renameThenUndo()
{
    QVERIFY(write("a.txt", "A"));
    RenameFileCommand cmd("a.txt", "sub/c.txt");
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QVERIFY(!exists("a.txt"));
    QCOMPARE(read("sub/c.txt"), QByteArray("A"));

    QVERIFY2(undo(&err), qPrintable(err));
    QCOMPARE(read("a.txt"), QByteArray("A"));
    QVERIFY(!exists("sub/c.txt"));
}

void TestFileCommands::undoRevertsInReverseOrder()
{
    QVERIFY(write("f.txt", "a\n"));
    QString err;
    EditInRangeCommand edit("f.txt", 1, 1, "b");
    QVERIFY2(edit.apply(m_dam.data(), &err), qPrintable(err));
    AppendToFileCommand append("f.txt", "c", true, true);
    QVERIFY2(append.apply(m_dam.data(), &err), qPrintable(err));
    CreateFileCommand create("g.txt", "g");
    QVERIFY2(create.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("b\nc"));

    QVERIFY2(undo(&err), qPrintable(err));
    QVERIFY(!exists("g.txt"));
    QCOMPARE(read("f.txt"), QByteArray("b\nc"));
    QVERIFY2(undo(&err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("b\n"));
    QVERIFY2(undo(&err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("a\n"));
    QVERIFY(!undo());
}

void TestFileCommands::undoWithoutHistoryFails()
{
    QString err;
    QVERIFY(!undo(&err));
    QVERIFY(!err.isEmpty());

    UndoLastChangeCommand withoutManager;
    QVERIFY(!withoutManager.apply(nullptr, &err));
}

/* ---- APPLY_PATCH ---- */

void TestFileCommands::applyPatchSingleHunk()
{
    QVERIFY(write("f.txt", "line1\nline2\nline3\n"));
    ApplyPatchCommand cmd("diff --git a/f.txt b/f.txt\n"
                          "--- a/f.txt\n"
                          "+++ b/f.txt\n"
                          "@@ -1,3 +1,3 @@\n"
                          " line1\n"
                          "-line2\n"
                          "+LINE2\n"
                          " line3\n", 0);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("line1\nLINE2\nline3\n"));

    QVERIFY2(undo(&err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("line1\nline2\nline3\n"));
}

void TestFileCommands::applyPatchCreatesFileFromDevNull()
{
    ApplyPatchCommand cmd("--- /dev/null\n"
                          "+++ b/new/created.txt\n"
                          "@@ -0,0 +1,2 @@\n"
                          "+hello\n"
                          "+world\n", 0);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("new/created.txt"), QByteArray("hello\nworld\n"));

    QVERIFY2(undo(&err), qPrintable(err));
    QVERIFY(!exists("new/created.txt"));
}

void TestFileCommands::applyPatchContextMismatchLeavesFileUntouched()
{
    QVERIFY(write("f.txt", "line1\nline2\nline3\n"));
    ApplyPatchCommand cmd("--- a/f.txt\n"
                          "+++ b/f.txt\n"
                          "@@ -1,3 +1,3 @@\n"
                          " something else\n"
                          "-line2\n"
                          "+LINE2\n"
                          " line3\n", 0);
    QString err;
    QVERIFY(!cmd.apply(m_dam.data(), &err));
    QVERIFY2(err.contains(QStringLiteral("mismatch")), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("line1\nline2\nline3\n"));
}

void TestFileCommands::applyPatchLinesThatLookLikeHeaders()
{
    // Removing the line "-- x" shows up as "--- x" in the diff, adding "++ y" as "+++ y".
    QVERIFY(write("f.txt", "a\n-- x\nb\n"));
    ApplyPatchCommand cmd("--- a/f.txt\n"
                          "+++ b/f.txt\n"
                          "@@ -1,3 +1,3 @@\n"
                          " a\n"
                          "--- x\n"
                          "+++ y\n"
                          " b\n", 0);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("a\n++ y\nb\n"));
}

void TestFileCommands::applyPatchHeaderWithTimestamp()
{
    QVERIFY(write("f.txt", "line1\nline2\nline3\n"));
    ApplyPatchCommand cmd("--- f.txt\t2024-01-01 10:00:00.000000000 +0000\n"
                          "+++ f.txt\t2024-01-02 11:00:00.000000000 +0000\n"
                          "@@ -2 +2 @@\n"
                          "-line2\n"
                          "+changed\n", 0);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("line1\nchanged\nline3\n"));
}

void TestFileCommands::applyPatchStripOne()
{
    QVERIFY(write("sub/f.txt", "old\n"));
    ApplyPatchCommand cmd("--- a/sub/f.txt\n"
                          "+++ b/sub/f.txt\n"
                          "@@ -1 +1 @@\n"
                          "-old\n"
                          "+new\n", 1);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("sub/f.txt"), QByteArray("new\n"));
    QVERIFY(!exists("b/sub/f.txt"));
}

void TestFileCommands::applyPatchTwoHunks()
{
    QVERIFY(write("n.txt", "1\n2\n3\n4\n5\n6\n7\n8\n"));
    ApplyPatchCommand cmd("--- a/n.txt\n"
                          "+++ b/n.txt\n"
                          "@@ -1,2 +1,3 @@\n"
                          " 1\n"
                          "+1.5\n"
                          " 2\n"
                          "@@ -6,2 +7,1 @@\n"
                          " 6\n"
                          "-7\n", 0);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("n.txt"), QByteArray("1\n1.5\n2\n3\n4\n5\n6\n8\n"));
}

void TestFileCommands::applyPatchKeepsCrlf()
{
    QVERIFY(write("f.txt", "line1\r\nline2\r\n"));
    ApplyPatchCommand cmd("--- a/f.txt\r\n"
                          "+++ b/f.txt\r\n"
                          "@@ -1,2 +1,2 @@\r\n"
                          " line1\r\n"
                          "-line2\r\n"
                          "+LINE2\r\n", 0);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(read("f.txt"), QByteArray("line1\r\nLINE2\r\n"));
}

void TestFileCommands::applyPatchFailureInSecondFileChangesNothing()
{
    QVERIFY(write("a.txt", "a\n"));
    QVERIFY(write("b.txt", "b\n"));
    ApplyPatchCommand cmd("--- a/a.txt\n"
                          "+++ b/a.txt\n"
                          "@@ -1 +1 @@\n"
                          "-a\n"
                          "+A\n"
                          "--- a/b.txt\n"
                          "+++ b/b.txt\n"
                          "@@ -1 +1 @@\n"
                          "-WRONG\n"
                          "+B\n", 0);
    QString err;
    QVERIFY(!cmd.apply(m_dam.data(), &err));
    QCOMPARE(read("a.txt"), QByteArray("a\n"));
    QCOMPARE(read("b.txt"), QByteArray("b\n"));
}

/* ---- OPEN_FILE_REGION ---- */

void TestFileCommands::openFileRegionLines()
{
    QVERIFY(write("f.txt", "one\ntwo\nthree\nfour\n"));
    OpenFileRegionCommand cmd("f.txt", QString(), 2, 3);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(cmd.content(), QStringLiteral("2: two\n3: three"));
}

void TestFileCommands::openFileRegionRegex()
{
    QVERIFY(write("f.txt", "one\ntwo\nthree\nfour\n"));
    OpenFileRegionCommand cmd("f.txt", QStringLiteral("^t"));
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QCOMPARE(cmd.content(), QStringLiteral("2: two\n3: three"));
}

void TestFileCommands::openFileRegionInvalidRegexFails()
{
    QVERIFY(write("f.txt", "one\ntwo\n"));
    OpenFileRegionCommand cmd("f.txt", QStringLiteral("([a-z"));
    QString err;
    QVERIFY(!cmd.apply(m_dam.data(), &err));
    QVERIFY2(err.contains(QStringLiteral("regex")), qPrintable(err));

    OpenFileRegionCommand nothing("f.txt");
    QVERIFY(!nothing.apply(m_dam.data(), &err));
}

/* ---- SEARCH_PROJECT ---- */

void TestFileCommands::searchIsCaseInsensitiveByDefault()
{
    QVERIFY(write("a.txt", "Hello World\n"));
    QVERIFY(write("sub/b.txt", "say hello\n"));
    QVERIFY(write("c.txt", "nothing\n"));
    QString err;

    SearchProjectCommand any(QStringLiteral("HELLO"));
    QVERIFY(any.apply(m_dam.data(), &err));
    QCOMPARE(any.matches().size(), 2);

    SearchProjectCommand exactCase(QStringLiteral("Hello"), 10, true);
    QVERIFY(exactCase.apply(m_dam.data(), &err));
    QCOMPARE(exactCase.matches().size(), 1);
    QVERIFY(exactCase.matches().first().contains(QStringLiteral("/a.txt:1")));

    SearchProjectCommand none(QStringLiteral("HELLO"), 10, true);
    QVERIFY(none.apply(m_dam.data(), &err));
    QVERIFY(none.matches().isEmpty());
}

void TestFileCommands::searchMaxMatches()
{
    QVERIFY(write("f.txt", "foo\nfoo\nfoo\nfoo\n"));
    SearchProjectCommand cmd(QStringLiteral("foo"), 2);
    QString err;
    QVERIFY(cmd.apply(m_dam.data(), &err));
    QCOMPARE(cmd.matches().size(), 2);
}

void TestFileCommands::searchRelativeExcludePaths()
{
    QVERIFY(write("build/out.txt", "needle\n"));
    QVERIFY(write("src/main.txt", "needle\n"));
    SearchProjectCommand cmd(QStringLiteral("needle"), 10, false, QStringList(), false, 0,
                             QStringList(), QStringList{QStringLiteral("build")});
    QString err;
    QVERIFY(cmd.apply(m_dam.data(), &err));
    QCOMPARE(cmd.matches().size(), 1);
    QVERIFY2(cmd.matches().first().contains(QStringLiteral("/src/main.txt")), qPrintable(cmd.matches().first()));
}

void TestFileCommands::searchRelativeIncludePaths()
{
    QVERIFY(write("build/out.txt", "needle\n"));
    QVERIFY(write("src/main.txt", "needle\n"));
    SearchProjectCommand cmd(QStringLiteral("needle"), 10, false, QStringList(), false, 0,
                             QStringList{QStringLiteral("src")});
    QString err;
    QVERIFY(cmd.apply(m_dam.data(), &err));
    QCOMPARE(cmd.matches().size(), 1);
    QVERIFY2(cmd.matches().first().contains(QStringLiteral("/src/main.txt")), qPrintable(cmd.matches().first()));
}

/* ---- DELETE_DIR ---- */

void TestFileCommands::deleteDirDotDotRefused()
{
    const QString marker = m_tmp->path() + QStringLiteral("/marker.txt");
    QVERIFY(TestFiles::write(marker, "keep"));
    QVERIFY(write("inside.txt", "keep"));

    QString err;
    DeleteDirCommand parent(QStringLiteral(".."), true);
    QVERIFY(!parent.apply(m_dam.data(), &err));
    QVERIFY(!err.isEmpty());

    DeleteDirCommand absoluteParent(m_tmp->path(), true);
    QVERIFY(!absoluteParent.apply(m_dam.data(), &err));

    DeleteDirCommand sneaky(QStringLiteral("sub/../.."), true);
    QVERIFY(!sneaky.apply(m_dam.data(), &err));

    QVERIFY(QFileInfo::exists(marker));
    QVERIFY(exists("inside.txt"));
}

void TestFileCommands::deleteDirProjectRootRefused()
{
    QVERIFY(write("inside.txt", "keep"));
    QString err;
    DeleteDirCommand dot(QStringLiteral("."), true);
    QVERIFY(!dot.apply(m_dam.data(), &err));
    DeleteDirCommand root(m_root, true);
    QVERIFY(!root.apply(m_dam.data(), &err));
    QVERIFY(exists("inside.txt"));
}

void TestFileCommands::deleteDirInsideProject()
{
    QVERIFY(write("sub/deeper/x.txt", "x"));
    DeleteDirCommand cmd(QStringLiteral("sub"), true);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QVERIFY(!QFileInfo::exists(abs("sub")));
}

void TestFileCommands::deleteEmptyDirThenUndo()
{
    QVERIFY(QDir().mkpath(abs("empty")));
    DeleteDirCommand cmd(QStringLiteral("empty"), false);
    QString err;
    QVERIFY2(cmd.apply(m_dam.data(), &err), qPrintable(err));
    QVERIFY(!QDir(abs("empty")).exists());
    QVERIFY2(undo(&err), qPrintable(err));
    QVERIFY(QDir(abs("empty")).exists());
}

/* ---- the project boundary ---- */

void TestFileCommands::changesOutsideProjectRefused()
{
    const QString outside = m_tmp->path() + QStringLiteral("/outside.txt");
    QString err;

    QVERIFY(!m_dam->applyChanges({Change::createFile(outside, "x")}, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!QFileInfo::exists(outside));

    QVERIFY(!m_dam->applyChanges({Change::createFile(QStringLiteral("../outside2.txt"), "x")}, &err));
    QVERIFY(!QFileInfo::exists(m_tmp->path() + QStringLiteral("/outside2.txt")));

    QVERIFY(write("in.txt", "in"));
    QVERIFY(!m_dam->applyChanges({Change::renameFile(abs("in.txt"), outside)}, &err));
    QCOMPARE(read("in.txt"), QByteArray("in"));

    const QString outsideSource = m_tmp->path() + QStringLiteral("/source.txt");
    QVERIFY(TestFiles::write(outsideSource, "src"));
    QVERIFY(!m_dam->applyChanges({Change::renameFile(outsideSource, abs("stolen.txt"))}, &err));
    QVERIFY(QFileInfo::exists(outsideSource));
    QVERIFY(!exists("stolen.txt"));

    // One change outside rejects the whole batch.
    QVERIFY(!m_dam->applyChanges({Change::createFile(abs("ok.txt"), "ok"), Change::createFile(outside, "x")}, &err));
    QVERIFY(!exists("ok.txt"));
}

void TestFileCommands::commandsOutsideProjectRefused()
{
    const QString outside = m_tmp->path() + QStringLiteral("/outside.txt");
    QVERIFY(TestFiles::write(outside, "a\nb\n"));
    QString err;

    CreateFileCommand create(m_tmp->path() + QStringLiteral("/evil.txt"), "x");
    QVERIFY(!create.apply(m_dam.data(), &err));
    QVERIFY(!QFileInfo::exists(m_tmp->path() + QStringLiteral("/evil.txt")));

    EditInRangeCommand edit(outside, 1, 1, "X");
    QVERIFY(!edit.apply(m_dam.data(), &err));

    AppendToFileCommand append(QStringLiteral("../outside.txt"), "c", true, true);
    QVERIFY(!append.apply(m_dam.data(), &err));

    ApplyPatchCommand patch("--- a/../outside.txt\n"
                            "+++ b/../outside.txt\n"
                            "@@ -1 +1 @@\n"
                            "-a\n"
                            "+A\n", 0);
    QVERIFY(!patch.apply(m_dam.data(), &err));

    DeleteFileCommand del(outside);
    QVERIFY(!del.apply(m_dam.data(), &err));

    QCOMPARE(TestFiles::read(outside), QByteArray("a\nb\n"));
}

void TestFileCommands::noProjectRefusesChanges()
{
    DevAssManager none;
    QString err;
    QVERIFY(!none.applyChanges({Change::createFile(abs("x.txt"), "x")}, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!exists("x.txt"));
}

QTEST_MAIN(TestFileCommands)
#include "tst_filecommands.moc"
