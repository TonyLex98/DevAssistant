#include "core/project/TextFileLines.h"

#include <QCoreApplication>
#include <QFile>

TextFileLines TextFileLines::fromBytes(const QByteArray& bytes)
{
    TextFileLines file;
    QByteArray data = bytes;
    if (data.startsWith("\xEF\xBB\xBF")) {
        file.bom = true;
        data.remove(0, 3);
    }
    file.eol = data.contains("\r\n") ? QByteArray("\r\n") : QByteArray("\n");
    if (data.isEmpty()) {
        file.trailingNewline = false;
        return file;
    }
    QString text = QString::fromUtf8(data);
    file.trailingNewline = text.endsWith(QLatin1Char('\n'));
    if (file.trailingNewline)
        text.chop(1);
    file.lines = text.split(QLatin1Char('\n'));
    for (QString& line : file.lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
    }
    return file;
}

bool TextFileLines::load(const QString& path, TextFileLines* out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QCoreApplication::translate("TextFileLines", "Cannot open %1 for reading").arg(path);
        return false;
    }
    *out = fromBytes(f.readAll());
    return true;
}

QByteArray TextFileLines::toBytes() const
{
    QByteArray out;
    if (bom)
        out += "\xEF\xBB\xBF";
    for (int i = 0; i < lines.size(); ++i) {
        if (i > 0)
            out += eol;
        out += lines.at(i).toUtf8();
    }
    if (trailingNewline && !lines.isEmpty())
        out += eol;
    return out;
}

QStringList splitInsertedText(const QString& text)
{
    if (text.isEmpty())
        return {};
    QString t = text;
    t.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (t.endsWith(QLatin1Char('\n')))
        t.chop(1);
    return t.split(QLatin1Char('\n'));
}
