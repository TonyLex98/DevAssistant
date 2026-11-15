// ReplaceInFileCommand.cpp
#include "core/commands/ReplaceInFileCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDateTime>

ReplaceInFileCommand::ReplaceInFileCommand(const QString& path,
                                          const QByteArray& content,
                                          const QString& descr)
{
    m_params["path"]    = path;
    m_params["content"] = QString::fromUtf8(content);
    m_description = descr.isEmpty()
        ? QString("Overwrites the ENTIRE content of an existing file. "
                  "Use this only if you want to replace the whole file. "
                  "For partial edits, use EDIT_IN_RANGE. "
                  "Example: { \"command\": \"REPLACE_IN_FILE\", \"params\": { \"path\": \"src/config.h\", \"content\": \"#define VERSION 2\" } }")
        : descr;
}

bool ReplaceInFileCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params["path"].toString();
    if (path.trimmed().isEmpty()) {
        if (err) *err = "REPLACE_IN_FILE: path is empty";
        return false;
    }
    
    // Normalize path relative to project root
    if (devAss && !QFileInfo(path).isAbsolute()) {
        path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
        // Update stored path for reporting
        m_params["path"] = path;
    }

    if (!QFileInfo::exists(path)) {
        if (err) *err = QString("REPLACE_IN_FILE: file not found: %1").arg(path);
        return false;
    }

    QByteArray cont = m_params["content"].toString().toUtf8();

    if (devAss) {
        QList<Change> changes;
        changes << Change::replaceInFile(path, cont);
        return devAss->applyChanges(changes, err);
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QString("Failed to open file for writing: %1").arg(path);
        return false;
    }
    f.write(cont);
    f.close();
    return true;
}

QJsonObject ReplaceInFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString ReplaceInFileCommand::getResult() const
{
    QString result;
    if (wasSuccessful()) {
        result = QString("REPLACE_IN_FILE: %1\n[status]\nFile content replaced successfully\nexitCode=0")
               .arg(m_params["path"].toString());
    } else {
        result = QString("REPLACE_IN_FILE: %1\n[status]\nFile content replacement failed\nexitCode=1")
               .arg(m_params["path"].toString());
    }
    
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}
