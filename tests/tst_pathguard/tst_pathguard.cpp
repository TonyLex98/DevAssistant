#include <QtTest>
#include <QDir>
#include <QTemporaryDir>

#include "PathGuard.h"
#include "TestFiles.h"

class TestPathGuard : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void relativePathInside();
    void backslashRelativePathInside();
    void dotDotEscapeRejected();
    void absolutePathOutsideRejected();
    void siblingWithSamePrefixRejected();
    void rootItselfIsInside();
    void notYetExistingNestedFileInside();
    void emptyProjectRootRejected();
    void emptyPathRejected();
    void caseInsensitiveOnWindows();

private:
    QTemporaryDir m_tmp;
    QString m_root;
};

void TestPathGuard::initTestCase()
{
    QVERIFY(m_tmp.isValid());
    m_root = m_tmp.path() + QStringLiteral("/project");
    QVERIFY(TestFiles::write(m_root + QStringLiteral("/src/main.cpp"), "int main() {}\n"));
    QVERIFY(QDir().mkpath(m_tmp.path() + QStringLiteral("/project2")));
}

void TestPathGuard::relativePathInside()
{
    QString abs, err;
    QVERIFY2(PathGuard::resolveInside(m_root, QStringLiteral("src/main.cpp"), &abs, &err), qPrintable(err));
    QCOMPARE(abs, QDir::cleanPath(m_root + QStringLiteral("/src/main.cpp")));
    QVERIFY(err.isEmpty());

    QVERIFY(PathGuard::resolveInside(m_root, QStringLiteral("./src/../src/main.cpp"), &abs, &err));
    QCOMPARE(abs, QDir::cleanPath(m_root + QStringLiteral("/src/main.cpp")));
    QVERIFY(PathGuard::isInside(m_root, m_root + QStringLiteral("/src/main.cpp")));

    // A name that only starts with two dots stays inside.
    QVERIFY2(PathGuard::resolveInside(m_root, QStringLiteral("..env"), &abs, &err), qPrintable(err));
    QCOMPARE(abs, m_root + QStringLiteral("/..env"));
}

void TestPathGuard::backslashRelativePathInside()
{
#ifdef Q_OS_WIN
    QString abs, err;
    QVERIFY2(PathGuard::resolveInside(m_root, QStringLiteral("src\\main.cpp"), &abs, &err), qPrintable(err));
    QCOMPARE(abs, QDir::cleanPath(m_root + QStringLiteral("/src/main.cpp")));
    QVERIFY(!PathGuard::resolveInside(m_root, QStringLiteral("..\\outside.txt"), &abs, &err));
#else
    QSKIP("Backslash is a path separator only on Windows");
#endif
}

void TestPathGuard::dotDotEscapeRejected()
{
    QString abs = QStringLiteral("untouched");
    QString err;
    QVERIFY(!PathGuard::resolveInside(m_root, QStringLiteral("../outside.txt"), &abs, &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(abs, QStringLiteral("untouched"));

    err.clear();
    QVERIFY(!PathGuard::resolveInside(m_root, QStringLiteral("src/../../outside.txt"), nullptr, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!PathGuard::resolveInside(m_root, QStringLiteral(".."), nullptr, nullptr));
    QVERIFY(!PathGuard::isInside(m_root, m_root + QStringLiteral("/src/../../outside.txt")));
}

void TestPathGuard::absolutePathOutsideRejected()
{
    QString err;
    const QString outside = m_tmp.path() + QStringLiteral("/elsewhere/file.txt");
    QVERIFY(!PathGuard::resolveInside(m_root, outside, nullptr, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!PathGuard::isInside(m_root, outside));
    QVERIFY(!PathGuard::isInside(m_root, m_tmp.path()));
}

void TestPathGuard::siblingWithSamePrefixRejected()
{
    // ".../project2" starts with ".../project" but is not inside it.
    const QString sibling = m_tmp.path() + QStringLiteral("/project2/file.txt");
    QVERIFY(!PathGuard::isInside(m_root, sibling));
    QVERIFY(!PathGuard::resolveInside(m_root, QStringLiteral("../project2/file.txt"), nullptr, nullptr));
    QVERIFY(!PathGuard::isInside(m_root, m_tmp.path() + QStringLiteral("/project2")));
}

void TestPathGuard::rootItselfIsInside()
{
    QVERIFY(PathGuard::isInside(m_root, m_root));
    QVERIFY(PathGuard::isInside(m_root + QStringLiteral("/"), m_root + QStringLiteral("/src")));

    QString abs, err;
    QVERIFY2(PathGuard::resolveInside(m_root, QStringLiteral("."), &abs, &err), qPrintable(err));
    QCOMPARE(abs, QDir::cleanPath(m_root));
    QVERIFY2(PathGuard::resolveInside(m_root, m_root, &abs, &err), qPrintable(err));
    QCOMPARE(abs, QDir::cleanPath(m_root));
}

void TestPathGuard::notYetExistingNestedFileInside()
{
    QString abs, err;
    QVERIFY2(PathGuard::resolveInside(m_root, QStringLiteral("new/deeper/file.txt"), &abs, &err), qPrintable(err));
    QCOMPARE(abs, QDir::cleanPath(m_root + QStringLiteral("/new/deeper/file.txt")));
    QVERIFY(!QFileInfo::exists(abs));
    QVERIFY(PathGuard::isInside(m_root, m_root + QStringLiteral("/a/b/c.txt")));
    // A missing path that climbs out is still outside.
    QVERIFY(!PathGuard::resolveInside(m_root, QStringLiteral("new/../../x/y.txt"), nullptr, nullptr));
}

void TestPathGuard::emptyProjectRootRejected()
{
    QString abs = QStringLiteral("untouched");
    QString err;
    QVERIFY(!PathGuard::resolveInside(QString(), QStringLiteral("a.txt"), &abs, &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(abs, QStringLiteral("untouched"));

    err.clear();
    QVERIFY(!PathGuard::resolveInside(QStringLiteral("   "), QStringLiteral("a.txt"), nullptr, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!PathGuard::isInside(QString(), m_root + QStringLiteral("/src/main.cpp")));
}

void TestPathGuard::emptyPathRejected()
{
    QString err;
    QVERIFY(!PathGuard::resolveInside(m_root, QString(), nullptr, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!PathGuard::resolveInside(m_root, QStringLiteral("  "), nullptr, nullptr));
    QVERIFY(!PathGuard::isInside(m_root, QString()));
}

void TestPathGuard::caseInsensitiveOnWindows()
{
#ifdef Q_OS_WIN
    QVERIFY(PathGuard::isInside(m_root.toUpper(), m_root.toLower() + QStringLiteral("/src/main.cpp")));
    QVERIFY(PathGuard::isInside(m_root.toLower(), m_root.toUpper() + QStringLiteral("/NEW/FILE.TXT")));
    QString abs, err;
    QVERIFY2(PathGuard::resolveInside(m_root.toUpper(), QStringLiteral("SRC/MAIN.CPP"), &abs, &err), qPrintable(err));
    QVERIFY(!PathGuard::isInside(m_root.toUpper(), m_tmp.path().toLower() + QStringLiteral("/project2/x.txt")));
#else
    QSKIP("Paths are case-insensitive only on Windows");
#endif
}

QTEST_MAIN(TestPathGuard)
#include "tst_pathguard.moc"
