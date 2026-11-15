#include <QtTest>
#include <QDir>
#include <QScopedPointer>
#include <QTemporaryDir>

#include "core/project/ChangeApplier.h"
#include "TestFiles.h"

class TestChangeApplier : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void appliesBatch();
    void batchIsAllOrNothing();
    void rollbackRestoresEveryKindOfChange();
    void failedBatchKeepsPreviousUndoStep();
    void multiLevelUndo();
    void undoOfReplaceOnNewFileDeletesIt();
    void undoRenameAndDirs();
    void undoWithEmptyHistoryFails();
    void historyIsCapped();

private:
    QString path(const QString& rel) const { return m_tmp->path() + QLatin1Char('/') + rel; }
    QByteArray read(const QString& rel) const { return TestFiles::read(path(rel)); }
    bool exists(const QString& rel) const { return QFileInfo::exists(path(rel)); }

    QScopedPointer<QTemporaryDir> m_tmp;
};

void TestChangeApplier::init()
{
    m_tmp.reset(new QTemporaryDir);
    QVERIFY(m_tmp->isValid());
}

void TestChangeApplier::cleanup()
{
    m_tmp.reset();
}

void TestChangeApplier::appliesBatch()
{
    QVERIFY(TestFiles::write(path("old.txt"), "old"));
    QVERIFY(TestFiles::write(path("gone.txt"), "gone"));

    ChangeApplier applier;
    QList<Change> batch{
        Change::createFile(path("dir/new.txt"), "new"),
        Change::replaceInFile(path("old.txt"), "replaced"),
        Change::deleteFile(path("gone.txt")),
        Change::createDir(path("empty")),
    };
    QString err;
    QVERIFY2(applier.apply(batch, &err), qPrintable(err));
    QCOMPARE(read("dir/new.txt"), QByteArray("new"));
    QCOMPARE(read("old.txt"), QByteArray("replaced"));
    QVERIFY(!exists("gone.txt"));
    QVERIFY(QDir(path("empty")).exists());
    QCOMPARE(applier.undoSteps(), 1);
    QVERIFY(applier.canUndo());

    QVERIFY2(applier.undo(&err), qPrintable(err));
    QVERIFY(!exists("dir/new.txt"));
    QCOMPARE(read("old.txt"), QByteArray("old"));
    QCOMPARE(read("gone.txt"), QByteArray("gone"));
    QVERIFY(!QDir(path("empty")).exists());
    QVERIFY(!applier.canUndo());
}

void TestChangeApplier::batchIsAllOrNothing()
{
    QVERIFY(TestFiles::write(path("existing.txt"), "keep"));

    ChangeApplier applier;
    QList<Change> batch{
        Change::createFile(path("first.txt"), "first"),
        Change::createFile(path("existing.txt"), "clobber"),  // fails: the file exists
    };
    QString err;
    QVERIFY(!applier.apply(batch, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY2(!exists("first.txt"), "the first change was not rolled back");
    QCOMPARE(read("existing.txt"), QByteArray("keep"));
    QCOMPARE(applier.undoSteps(), 0);
}

void TestChangeApplier::rollbackRestoresEveryKindOfChange()
{
    QVERIFY(TestFiles::write(path("edited.txt"), "before"));
    QVERIFY(TestFiles::write(path("deleted.txt"), QByteArray("bytes\r\n\0end", 11)));
    QVERIFY(TestFiles::write(path("moved.txt"), "moved"));

    ChangeApplier applier;
    QList<Change> batch{
        Change::replaceInFile(path("edited.txt"), "after"),
        Change::deleteFile(path("deleted.txt")),
        Change::renameFile(path("moved.txt"), path("sub/moved.txt")),
        Change::renameFile(path("missing.txt"), path("target.txt")),  // fails: no source
    };
    QVERIFY(!applier.apply(batch, nullptr));
    QCOMPARE(read("edited.txt"), QByteArray("before"));
    QCOMPARE(read("deleted.txt"), QByteArray("bytes\r\n\0end", 11));
    QCOMPARE(read("moved.txt"), QByteArray("moved"));
    QVERIFY(!exists("sub/moved.txt"));
    QVERIFY(!exists("target.txt"));
}

void TestChangeApplier::failedBatchKeepsPreviousUndoStep()
{
    ChangeApplier applier;
    QList<Change> first{Change::createFile(path("a.txt"), "A")};
    QVERIFY(applier.apply(first, nullptr));
    QCOMPARE(applier.undoSteps(), 1);

    QList<Change> failing{
        Change::createFile(path("b.txt"), "B"),
        Change::createFile(path("a.txt"), "again"),  // fails
    };
    QVERIFY(!applier.apply(failing, nullptr));
    QVERIFY(!exists("b.txt"));
    QCOMPARE(read("a.txt"), QByteArray("A"));
    QCOMPARE(applier.undoSteps(), 1);

    QString err;
    QVERIFY2(applier.undo(&err), qPrintable(err));
    QVERIFY2(!exists("a.txt"), "undo did not revert the last successful batch");
    QCOMPARE(applier.undoSteps(), 0);
}

void TestChangeApplier::multiLevelUndo()
{
    ChangeApplier applier;
    QList<Change> b1{Change::createFile(path("f.txt"), "1")};
    QList<Change> b2{Change::replaceInFile(path("f.txt"), "2")};
    QList<Change> b3{Change::replaceInFile(path("f.txt"), "3"), Change::createFile(path("g.txt"), "g")};
    QVERIFY(applier.apply(b1, nullptr));
    QVERIFY(applier.apply(b2, nullptr));
    QVERIFY(applier.apply(b3, nullptr));
    QCOMPARE(applier.undoSteps(), 3);
    QCOMPARE(read("f.txt"), QByteArray("3"));

    QVERIFY(applier.undo(nullptr));
    QCOMPARE(read("f.txt"), QByteArray("2"));
    QVERIFY(!exists("g.txt"));

    QVERIFY(applier.undo(nullptr));
    QCOMPARE(read("f.txt"), QByteArray("1"));

    QVERIFY(applier.undo(nullptr));
    QVERIFY(!exists("f.txt"));
    QVERIFY(!applier.canUndo());
}

void TestChangeApplier::undoOfReplaceOnNewFileDeletesIt()
{
    ChangeApplier applier;
    QList<Change> batch{Change::replaceInFile(path("fresh/new.txt"), "content")};
    QVERIFY(applier.apply(batch, nullptr));
    QCOMPARE(read("fresh/new.txt"), QByteArray("content"));

    QString err;
    QVERIFY2(applier.undo(&err), qPrintable(err));
    QVERIFY2(!exists("fresh/new.txt"), "undo left behind a file that did not exist before");
}

void TestChangeApplier::undoRenameAndDirs()
{
    QVERIFY(TestFiles::write(path("a.txt"), "A"));
    QVERIFY(QDir().mkpath(path("emptydir")));

    ChangeApplier applier;
    QList<Change> batch{
        Change::renameFile(path("a.txt"), path("moved/b.txt")),
        Change::deleteDir(path("emptydir")),
        Change::deleteDir(path("never-existed")),  // no-op
    };
    QString err;
    QVERIFY2(applier.apply(batch, &err), qPrintable(err));
    QVERIFY(!exists("a.txt"));
    QCOMPARE(read("moved/b.txt"), QByteArray("A"));
    QVERIFY(!QDir(path("emptydir")).exists());

    QVERIFY2(applier.undo(&err), qPrintable(err));
    QCOMPARE(read("a.txt"), QByteArray("A"));
    QVERIFY(!exists("moved/b.txt"));
    QVERIFY(QDir(path("emptydir")).exists());
    QVERIFY(!QDir(path("never-existed")).exists());
}

void TestChangeApplier::undoWithEmptyHistoryFails()
{
    ChangeApplier applier;
    QVERIFY(!applier.canUndo());
    QString err;
    QVERIFY(!applier.undo(&err));
    QVERIFY(!err.isEmpty());

    // An empty batch is a success that adds no undo step.
    QList<Change> none;
    QVERIFY(applier.apply(none, nullptr));
    QVERIFY(!applier.canUndo());
}

void TestChangeApplier::historyIsCapped()
{
    ChangeApplier applier;
    for (int i = 0; i < ChangeApplier::kMaxUndoSteps + 5; ++i) {
        QList<Change> batch{Change::replaceInFile(path("f.txt"), QByteArray::number(i))};
        QVERIFY(applier.apply(batch, nullptr));
    }
    QCOMPARE(applier.undoSteps(), int(ChangeApplier::kMaxUndoSteps));
}

QTEST_MAIN(TestChangeApplier)
#include "tst_changeapplier.moc"
