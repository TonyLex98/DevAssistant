// DeleteFileCommand.cpp
#include "core/commands/DeleteFileCommand.h"
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QDir>

DeleteFileCommand::DeleteFileCommand(const QString& path,
                                     const QString& descr)
{
    m_params["path"] = path;
    m_description = descr.isEmpty()
        ? QString("Permanently deletes a file. "
                  "Example: { \"command\": \"DELETE_FILE\", \"params\": { \"path\": \"src/temp.cpp\" } }")
        : descr;
}

bool DeleteFileCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params["path"].toString();
    if (path.trimmed().isEmpty()) {
        if (err) *err = "DELETE_FILE: path is empty";
        return false;
    }
    
    // Normalize path relative to project root
    if (devAss && !QFileInfo(path).isAbsolute()) {
        path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
        // Update stored path for reporting
        m_params["path"] = path;
    }

    if (!QFileInfo::exists(path)) {
        if (err) *err = QString("DELETE_FILE: file not found: %1").arg(path);
        return false;
    }

    if (devAss) {
        QList<Change> changes;
        changes << Change::deleteFile(path);
        return devAss->applyChanges(changes, err);
    }

    if (!QFile::remove(path)) {
        if (err) *err = QString("Failed to delete file %1").arg(path);
        return false;
    }
    return true;
}

QJsonObject DeleteFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString DeleteFileCommand::getResult() const
{
    QString result;
    if (wasSuccessful()) {
        result = QString("DELETE_FILE: %1\n[status]\nFile deleted successfully\nexitCode=0")
               .arg(m_params["path"].toString());
    } else {
        result = QString("DELETE_FILE: %1\n[status]\nFile deletion failed\nexitCode=1")
               .arg(m_params["path"].toString());
    }
    
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}
