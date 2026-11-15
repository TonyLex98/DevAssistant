#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>

// A text file as lines, read and written so that an edit changes only the lines it touches:
// the file is decoded as UTF-8 (a byte-order mark is kept), and its line endings (LF or CRLF)
// and whether it ends with a newline are written back as they were.
struct TextFileLines
{
    QStringList lines;
    QByteArray eol = "\n";
    bool trailingNewline = true;
    bool bom = false;

    static bool load(const QString& path, TextFileLines* out, QString* error);
    static TextFileLines fromBytes(const QByteArray& bytes);
    QByteArray toBytes() const;
};

// Splits text that is about to be inserted into lines. One trailing newline ends the last
// line instead of adding an empty one: "a\nb\n" -> {"a", "b"}; "" -> {}.
QStringList splitInsertedText(const QString& text);
