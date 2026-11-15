#include <QtTest>
#include <QDir>
#include <QTemporaryDir>

#include "core/util/json_utils.h"

using ByteArrayList = QList<QByteArray>;

class TestJsonUtils : public QObject
{
    Q_OBJECT

private slots:
    void splitConcatenatedJson_data();
    void splitConcatenatedJson();

    void normalizeContextPath();
    void displayContextPath();
};

void TestJsonUtils::splitConcatenatedJson_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<ByteArrayList>("parts");

    QTest::newRow("stray brace before JSON")
            << QStringLiteral("Done } here it is: {\"a\":1}")
            << ByteArrayList{"{\"a\":1}"};
    QTest::newRow("stray brackets before JSON")
            << QStringLiteral("]] } {\"a\":[1,2]}")
            << ByteArrayList{"{\"a\":[1,2]}"};
    QTest::newRow("array then object")
            << QStringLiteral("[1,2] and {\"b\":\"x}\"}")
            << ByteArrayList{"[1,2]", "{\"b\":\"x}\"}"};
    QTest::newRow("nested")
            << QStringLiteral("{\"a\":{\"b\":[1,{\"c\":2}]}}")
            << ByteArrayList{"{\"a\":{\"b\":[1,{\"c\":2}]}}"};
    QTest::newRow("escaped quote in a string")
            << QStringLiteral("{\"a\":\"say \\\"}\\\" ok\"}")
            << ByteArrayList{"{\"a\":\"say \\\"}\\\" ok\"}"};
    QTest::newRow("concatenated objects")
            << QStringLiteral("{\"a\":1}{\"b\":2}\n{\"c\":3}")
            << ByteArrayList{"{\"a\":1}", "{\"b\":2}", "{\"c\":3}"};
    QTest::newRow("Cyrillic")
            << QStringLiteral("текст {\"t\":\"привет\"}")
            << ByteArrayList{QByteArray("{\"t\":\"привет\"}")};
    QTest::newRow("prose only") << QStringLiteral("no JSON here") << ByteArrayList();
    QTest::newRow("unclosed") << QStringLiteral("{\"a\":1") << ByteArrayList();
    QTest::newRow("empty") << QString() << ByteArrayList();
}

void TestJsonUtils::splitConcatenatedJson()
{
    QFETCH(QString, text);
    QFETCH(ByteArrayList, parts);
    QCOMPARE(splitConcatenatedJson_fromText(text), parts);
}

void TestJsonUtils::normalizeContextPath()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString root = tmp.path();

    QCOMPARE(::normalizeContextPath(QString(), root), QString());
    QCOMPARE(::normalizeContextPath(QStringLiteral("   "), root), QString());
    QCOMPARE(::normalizeContextPath(QStringLiteral("src/a.cpp"), root), root + QStringLiteral("/src/a.cpp"));
    QCOMPARE(::normalizeContextPath(root + QStringLiteral("/src/../src/./a.cpp"), root),
             root + QStringLiteral("/src/a.cpp"));
    QCOMPARE(::normalizeContextPath(tmp.path() + QStringLiteral("/../other/b.cpp"), root),
             QDir::cleanPath(tmp.path() + QStringLiteral("/../other/b.cpp")));
}

void TestJsonUtils::displayContextPath()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString root = tmp.path() + QStringLiteral("/project");

    QCOMPARE(::displayContextPath(root + QStringLiteral("/src/a.cpp"), root),
             QDir::toNativeSeparators(QStringLiteral("src/a.cpp")));
    QCOMPARE(::displayContextPath(root + QStringLiteral("/a.cpp"), root), QStringLiteral("a.cpp"));

    const QString outside = tmp.path() + QStringLiteral("/other/b.cpp");
    QCOMPARE(::displayContextPath(outside, root), QDir::toNativeSeparators(outside));

    // Round trip: a relative path normalized and displayed again.
    QCOMPARE(::displayContextPath(::normalizeContextPath(QStringLiteral("src/main.cpp"), root), root),
             QDir::toNativeSeparators(QStringLiteral("src/main.cpp")));

    // A name that only starts with two dots is inside the project.
    QCOMPARE(::displayContextPath(root + QStringLiteral("/..env"), root), QStringLiteral("..env"));
}

QTEST_MAIN(TestJsonUtils)
#include "tst_jsonutils.moc"
