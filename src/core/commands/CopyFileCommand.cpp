// CopyFileCommand.cpp
#include "core/commands/CopyFileCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>

CopyFileCommand::CopyFileCommand(const QString& fromPath,
                                 const QString& toPath,
                                 bool overwrite,
                                 const QString& descr)
{
    m_params["from"] = fromPath;
    m_params["to"] = toPath;
    m_params["overwrite"] = overwrite;
    m_description = descr.isEmpty()
        ? QStringLiteral("Copies a file. Creates parent directories for the destination. "
                         "Example: { \"command\": \"COPY_FILE\", \"params\": { \"from\": \"src/a.txt\", \"to\": \"src/b.txt\", \"overwrite\": false } }")
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

bool CopyFileCommand::apply(DevAssManager* devAss, QString* err)
{
    m_stdout.clear();
    QString fromPath = m_params.value("from").toString().trimmed();
    QString toPath = m_params.value("to").toString().trimmed();
    const bool overwrite = m_params.value("overwrite").toBool(false);

    if (fromPath.isEmpty() || toPath.isEmpty()) {
        if (err) *err = "COPY_FILE: missing from/to";
        return false;
    }

    fromPath = normalizePathMaybe(devAss, fromPath);
    toPath = normalizePathMaybe(devAss, toPath);
    m_params["from"] = fromPath;
    m_params["to"] = toPath;

    QFileInfo srcInfo(fromPath);
    if (!srcInfo.exists() || !srcInfo.isFile()) {
        if (err) *err = QString("COPY_FILE: source file not found: %1").arg(fromPath);
        return false;
    }

    QFileInfo dstInfo(toPath);
    if (dstInfo.exists() && !overwrite) {
        if (err) *err = QString("COPY_FILE: destination already exists: %1").arg(toPath);
        return false;
    }

    QFile src(fromPath);
    if (!src.open(QIODevice::ReadOnly)) {
        if (err) *err = QString("COPY_FILE: cannot read source file: %1").arg(fromPath);
        return false;
    }
    const QByteArray bytes = src.readAll();
    src.close();

    if (devAss) {
        QList<Change> changes;
        if (dstInfo.exists())
            changes << Change::replaceInFile(toPath, bytes);
        else
            changes << Change::createFile(toPath, bytes);
        return devAss->applyChanges(changes, err);
    }

    QDir dir = QFileInfo(toPath).absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (err) *err = QString("COPY_FILE: cannot create destination directory: %1").arg(dir.path());
        return false;
    }
    QFile dst(toPath);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QString("COPY_FILE: cannot write destination file: %1").arg(toPath);
        return false;
    }
    dst.write(bytes);
    dst.close();
    return true;
}

QJsonObject CopyFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString CopyFileCommand::getResult() const
{
    const bool ok = wasSuccessful();
    const QString fromPath = m_params.value("from").toString();
    const QString toPath = m_params.value("to").toString();
    QString result = QString("COPY_FILE: %1 -> %2\n[status]\n%3\nexitCode=%4")
                         .arg(fromPath, toPath)
                         .arg(ok ? "File copied successfully" : "File copy failed")
                         .arg(ok ? 0 : 1);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

