// RenameFileCommand.cpp
#include "RenameFileCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QDateTime>

RenameFileCommand::RenameFileCommand(const QString& oldPath,
                                     const QString& newPath,
                                     const QString& descr)
{
    m_params["oldPath"] = oldPath;
    m_params["newPath"] = newPath;
    m_description = descr.isEmpty()
        ? QString("Renames or moves a file. "
                  "Example: { \"command\": \"RENAME_FILE\", \"params\": { \"oldPath\": \"src/old.cpp\", \"newPath\": \"src/new.cpp\" } }")
        : descr;
}

bool RenameFileCommand::apply(DevAssManager* devAss, QString* err)
{
    QString oldPath = m_params["oldPath"].toString();
    QString newPath = m_params["newPath"].toString();

    if (oldPath.trimmed().isEmpty() || newPath.trimmed().isEmpty()) {
        if (err) *err = "Missing oldPath or newPath for rename";
        return false;
    }

    // Resolve relative paths against the project root
    if (devAss) {
        if (!QFileInfo(oldPath).isAbsolute()) {
            oldPath = QDir(devAss->projectRoot().absolutePath()).filePath(oldPath);
        }
        if (!QFileInfo(newPath).isAbsolute()) {
            newPath = QDir(devAss->projectRoot().absolutePath()).filePath(newPath);
        }
        // Store the resolved paths so the result shows them
        m_params["oldPath"] = oldPath;
        m_params["newPath"] = newPath;
    }

    if (!QFileInfo::exists(oldPath)) {
        if (err) *err = QString("Source file does not exist: %1").arg(oldPath);
        return false;
    }

    if (QFileInfo::exists(newPath)) {
        if (err) *err = QString("Target path already exists: %1").arg(newPath);
        return false;
    }

    if (devAss) {
        QList<Change> changes;
        changes << Change::renameFile(oldPath, newPath);
        return devAss->applyChanges(changes, err);
    }

    // Create missing parent directories for the new path
    QDir newDir = QFileInfo(newPath).absoluteDir();
    if (!newDir.exists() && !newDir.mkpath(".")) {
        if (err) *err = QString("Cannot create the directory for the new file: %1").arg(newDir.path());
        return false;
    }

    if (!QFile::rename(oldPath, newPath)) {
        if (err) *err = QString("Cannot rename %1 to %2")
                        .arg(oldPath).arg(newPath);
        return false;
    }
    return true;
}

QJsonObject RenameFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString RenameFileCommand::getResult() const
{
    const QString oldPath = m_params.value("oldPath").toString();
    const QString newPath = m_params.value("newPath").toString();
    const bool ok = wasSuccessful();

    QString result = QString("RENAME_FILE: %1 -> %2\n[status]\n%3\nexitCode=%4")
            .arg(oldPath, newPath)
            .arg(ok ? "File moved/renamed successfully" : "File move/rename failed")
            .arg(ok ? 0 : 1);

    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    return result;
}
