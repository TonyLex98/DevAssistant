#include <QtTest>
#include <QTemporaryDir>

#include "core/project/TextFileLines.h"
#include "TestFiles.h"

class TestTextFileLines : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip_data();
    void roundTrip();
    void editKeepsFormat();
    void load();
    void loadMissingFileFails();
    void defaultIsEmpty();
    void splitInsertedText_data();
    void splitInsertedText();
};

void TestTextFileLines::roundTrip_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QStringList>("lines");
    QTest::addColumn<QByteArray>("eol");
    QTest::addColumn<bool>("trailingNewline");
    QTest::addColumn<bool>("bom");

    QTest::newRow("LF") << QByteArray("a\nb\n") << QStringList{"a", "b"} << QByteArray("\n") << true << false;
    QTest::newRow("CRLF") << QByteArray("a\r\nb\r\n") << QStringList{"a", "b"} << QByteArray("\r\n") << true << false;
    QTest::newRow("BOM") << QByteArray("\xEF\xBB\xBF" "a\nb\n") << QStringList{"a", "b"} << QByteArray("\n") << true << true;
    QTest::newRow("BOM and CRLF") << QByteArray("\xEF\xBB\xBF" "a\r\nb\r\n") << QStringList{"a", "b"}
                                  << QByteArray("\r\n") << true << true;
    QTest::newRow("no final newline") << QByteArray("a\nb") << QStringList{"a", "b"} << QByteArray("\n") << false << false;
    QTest::newRow("CRLF, no final newline") << QByteArray("a\r\nb") << QStringList{"a", "b"}
                                            << QByteArray("\r\n") << false << false;
    QTest::newRow("empty file") << QByteArray() << QStringList() << QByteArray("\n") << false << false;
    QTest::newRow("BOM only") << QByteArray("\xEF\xBB\xBF") << QStringList() << QByteArray("\n") << false << true;
    QTest::newRow("single newline") << QByteArray("\n") << QStringList{QString()} << QByteArray("\n") << true << false;
    QTest::newRow("blank lines kept") << QByteArray("a\n\nb\n\n") << QStringList{"a", "", "b", ""}
                                      << QByteArray("\n") << true << false;
    QTest::newRow("UTF-8") << QByteArray("Привет\nмир\n")
                           << QStringList{QStringLiteral("Привет"), QStringLiteral("мир")}
                           << QByteArray("\n") << true << false;
}

void TestTextFileLines::roundTrip()
{
    QFETCH(QByteArray, bytes);
    QFETCH(QStringList, lines);
    QFETCH(QByteArray, eol);
    QFETCH(bool, trailingNewline);
    QFETCH(bool, bom);

    const TextFileLines file = TextFileLines::fromBytes(bytes);
    QCOMPARE(file.lines, lines);
    QCOMPARE(file.eol, eol);
    QCOMPARE(file.trailingNewline, trailingNewline);
    QCOMPARE(file.bom, bom);
    QCOMPARE(file.toBytes(), bytes);
}

void TestTextFileLines::editKeepsFormat()
{
    TextFileLines file = TextFileLines::fromBytes(QByteArray("\xEF\xBB\xBF" "one\r\ntwo\r\nthree"));
    QCOMPARE(file.lines.size(), 3);
    file.lines[1] = QStringLiteral("два");
    file.lines.insert(1, QStringLiteral("inserted"));
    QCOMPARE(file.toBytes(), QByteArray("\xEF\xBB\xBF" "one\r\ninserted\r\nдва\r\nthree"));
}

void TestTextFileLines::load()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("f.txt"));
    QVERIFY(TestFiles::write(path, QByteArray("x\r\ny\r\n")));

    TextFileLines file;
    QString err;
    QVERIFY2(TextFileLines::load(path, &file, &err), qPrintable(err));
    QCOMPARE(file.lines, (QStringList{"x", "y"}));
    QCOMPARE(file.eol, QByteArray("\r\n"));
    QVERIFY(file.trailingNewline);
}

void TestTextFileLines::loadMissingFileFails()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    TextFileLines file;
    QString err;
    QVERIFY(!TextFileLines::load(tmp.filePath(QStringLiteral("missing.txt")), &file, &err));
    QVERIFY(!err.isEmpty());
}

void TestTextFileLines::defaultIsEmpty()
{
    const TextFileLines file;
    QVERIFY(file.lines.isEmpty());
    QCOMPARE(file.toBytes(), QByteArray());
}

void TestTextFileLines::splitInsertedText_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QStringList>("lines");

    QTest::newRow("empty") << QString() << QStringList();
    QTest::newRow("one line") << QStringLiteral("a") << QStringList{"a"};
    QTest::newRow("one line, newline") << QStringLiteral("a\n") << QStringList{"a"};
    QTest::newRow("two lines, newline") << QStringLiteral("a\nb\n") << QStringList{"a", "b"};
    QTest::newRow("CRLF") << QStringLiteral("a\r\nb\r\n") << QStringList{"a", "b"};
    QTest::newRow("just a newline") << QStringLiteral("\n") << QStringList{QString()};
    QTest::newRow("trailing blank line") << QStringLiteral("a\n\n") << QStringList{"a", ""};
    QTest::newRow("leading blank lines") << QStringLiteral("\n\na") << QStringList{"", "", "a"};
}

void TestTextFileLines::splitInsertedText()
{
    QFETCH(QString, text);
    QFETCH(QStringList, lines);
    QCOMPARE(::splitInsertedText(text), lines);
}

QTEST_MAIN(TestTextFileLines)
#include "tst_textfilelines.moc"
