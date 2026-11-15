// DeleteDirCommand.cpp
#include "core/commands/DeleteDirCommand.h"
#include <QDir>
#include <QFileInfo>

DeleteDirCommand::DeleteDirCommand(const QString& path, bool recursive, const QString& descr)
{
    m_params["path"] = path;
    m_params["recursive"] = recursive;
    m_description = descr.isEmpty()
        ? QStringLiteral("Deletes a directory. By default, only deletes empty directories. "
                         "If recursive=true, deletes contents (UNDO may fail). "
                         "Example: { \"command\": \"DELETE_DIR\", \"params\": { \"path\": \"build/tmp\", \"recursive\": false } }")
        : descr;
}

static QString normalizeDirPathMaybe(DevAssManager* devAss, const QString& p)
{
    if (!devAss)
        return p;
    if (QFileInfo(p).isAbsolute())
        return QDir::cleanPath(p);
    return QDir(devAss->projectRoot().absolutePath()).filePath(p);
}

bool DeleteDirCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params.value("path").toString().trimmed();
    const bool recursive = m_params.value("recursive").toBool(false);
    if (path.isEmpty()) {
        if (err) *err = "DELETE_DIR: path is empty";
        return false;
    }

    path = normalizeDirPathMaybe(devAss, path);
    m_params["path"] = path;

    // Safety: avoid deleting the project root directory.
    if (devAss && QDir::cleanPath(path) == QDir::cleanPath(devAss->projectRoot().absolutePath())) {
        if (err) *err = "DELETE_DIR: refusing to delete project root";
        return false;
    }

    if (devAss) {
        QList<Change> changes;
        changes << Change::deleteDir(path, recursive);
        return devAss->applyChanges(changes, err);
    }

    QDir d(path);
    if (!d.exists()) {
        if (err) *err = QString("DELETE_DIR: directory not found: %1").arg(path);
        return false;
    }
    bool ok = false;
    if (recursive)
        ok = d.removeRecursively();
    else
        ok = d.rmdir(path);
    if (!ok) {
        if (err) *err = QString("DELETE_DIR: cannot delete directory: %1").arg(path);
        return false;
    }
    return true;
}

QJsonObject DeleteDirCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString DeleteDirCommand::getResult() const
{
    const bool ok = wasSuccessful();
    const QString path = m_params.value("path").toString();
    const bool recursive = m_params.value("recursive").toBool(false);
    QString result = QString("DELETE_DIR: %1\n[status]\n%2\n[recursive]\n%3\nexitCode=%4")
                         .arg(path)
                         .arg(ok ? "Directory deleted successfully" : "Directory deletion failed")
                         .arg(recursive ? "true" : "false")
                         .arg(ok ? 0 : 1);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

