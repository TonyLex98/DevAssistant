// InsertAtLineCommand.cpp
#include "InsertAtLineCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>

static QString normalizePathMaybe(DevAssManager* devAss, const QString& p)
{
    if (!devAss)
        return p;
    if (QFileInfo(p).isAbsolute())
        return QDir::cleanPath(p);
    return QDir(devAss->projectRoot().absolutePath()).filePath(p);
}

static QStringList splitPreserveEmpty(const QString& text)
{
    return text.split('\n', QString::KeepEmptyParts);
}

InsertAtLineCommand::InsertAtLineCommand(const QString& path,
                                         int line,
                                         const QByteArray& content,
                                         const QString& descr)
{
    m_params["path"] = path;
    m_params["line"] = line;
    m_params["content"] = QString::fromUtf8(content);
    m_description = descr.isEmpty()
        ? QStringLiteral("Inserts text before the given 1-based line number. "
                         "Use OPEN_FILE first to get exact line numbers. "
                         "Example: { \"command\": \"INSERT_AT_LINE\", \"params\": { \"path\": \"src/main.cpp\", \"line\": 10, \"content\": \"// inserted\\n\" } }")
        : descr;
}

bool InsertAtLineCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params.value("path").toString().trimmed();
    const int line = m_params.value("line").toInt(0);
    const QString contentText = m_params.value("content").toString();

    if (path.isEmpty() || line <= 0) {
        if (err) *err = "INSERT_AT_LINE: missing path or invalid line";
        return false;
    }

    path = normalizePathMaybe(devAss, path);
    m_params["path"] = path;

    if (!QFileInfo::exists(path)) {
        if (err) *err = QString("INSERT_AT_LINE: file not found: %1").arg(path);
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QString("INSERT_AT_LINE: cannot read file: %1").arg(path);
        return false;
    }
    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd())
        lines << in.readLine();
    f.close();

    if (line > lines.size() + 1) {
        if (err) *err = QString("INSERT_AT_LINE: line %1 is beyond end of file (%2 lines)").arg(line).arg(lines.size());
        return false;
    }

    QStringList toInsert = splitPreserveEmpty(contentText);
    if (contentText.endsWith('\n') && !toInsert.isEmpty())
        toInsert.removeLast();

    const int idx = qMax(0, qMin(line - 1, lines.size()));
    for (int i = 0; i < toInsert.size(); ++i) {
        lines.insert(idx + i, toInsert.at(i));
    }

    QByteArray outBytes;
    for (const QString& l : lines) {
        outBytes += l.toUtf8();
        outBytes += '\n';
    }

    if (devAss) {
        QList<Change> changes;
        changes << Change::replaceInFile(path, outBytes);
        return devAss->applyChanges(changes, err);
    }

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QString("INSERT_AT_LINE: cannot write file: %1").arg(path);
        return false;
    }
    out.write(outBytes);
    out.close();
    return true;
}

QJsonObject InsertAtLineCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString InsertAtLineCommand::getResult() const
{
    const bool ok = wasSuccessful();
    const QString path = m_params.value("path").toString();
    const int line = m_params.value("line").toInt();
    QString result = QString("INSERT_AT_LINE: %1 @%2\n[status]\n%3\nexitCode=%4")
                         .arg(path)
                         .arg(line)
                         .arg(ok ? "Insert successful" : "Insert failed")
                         .arg(ok ? 0 : 1);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

