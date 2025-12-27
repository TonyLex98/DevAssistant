// AppendToFileCommand.cpp
#include "AppendToFileCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>

AppendToFileCommand::AppendToFileCommand(const QString& path,
                                         const QByteArray& content,
                                         bool createIfMissing,
                                         bool ensureNewline,
                                         const QString& descr)
{
    m_params["path"] = path;
    m_params["content"] = QString::fromUtf8(content);
    m_params["createIfMissing"] = createIfMissing;
    m_params["ensureNewline"] = ensureNewline;
    m_description = descr.isEmpty()
        ? QStringLiteral("Appends text to a file. "
                         "Example: { \"command\": \"APPEND_TO_FILE\", \"params\": { \"path\": \"README.md\", \"content\": \"\\nMore...\", \"createIfMissing\": true } }")
        : descr;
}

static QString normalizePathMaybe(DevAssManager* devAss, const QString& p)
{
    if (!devAss)
        return p;
    if (QFileInfo(p).isAbsolute())
        return QDir::cleanPath(p);
    return QDir(devAss->projectRoot().absolutePath()).filePath(p);
}

bool AppendToFileCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params.value("path").toString().trimmed();
    if (path.isEmpty()) {
        if (err) *err = "APPEND_TO_FILE: path is empty";
        return false;
    }

    path = normalizePathMaybe(devAss, path);
    m_params["path"] = path;

    const bool createIfMissing = m_params.value("createIfMissing").toBool(true);
    const bool ensureNewline = m_params.value("ensureNewline").toBool(false);
    const QByteArray addBytes = m_params.value("content").toString().toUtf8();

    QFileInfo info(path);
    const bool exists = info.exists();
    if (!exists && !createIfMissing) {
        if (err) *err = QString("APPEND_TO_FILE: file not found: %1").arg(path);
        return false;
    }

    QByteArray base;
    if (exists) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            if (err) *err = QString("APPEND_TO_FILE: cannot read file: %1").arg(path);
            return false;
        }
        base = f.readAll();
        f.close();
    }

    QByteArray out = base;
    if (ensureNewline && !out.isEmpty() && !out.endsWith('\n') && !addBytes.startsWith('\n'))
        out += '\n';
    out += addBytes;

    if (devAss) {
        QList<Change> changes;
        if (exists)
            changes << Change::replaceInFile(path, out);
        else
            changes << Change::createFile(path, out);
        return devAss->applyChanges(changes, err);
    }

    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (err) *err = QString("APPEND_TO_FILE: cannot create directory: %1").arg(dir.path());
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QString("APPEND_TO_FILE: cannot write file: %1").arg(path);
        return false;
    }
    f.write(out);
    f.close();
    return true;
}

QJsonObject AppendToFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString AppendToFileCommand::getResult() const
{
    const bool ok = wasSuccessful();
    const QString path = m_params.value("path").toString();
    QString result = QString("APPEND_TO_FILE: %1\n[status]\n%2\nexitCode=%3")
                         .arg(path)
                         .arg(ok ? "Append successful" : "Append failed")
                         .arg(ok ? 0 : 1);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

