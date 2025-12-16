// ListFilesInFolderCommand.cpp
#include "ListFilesInFolderCommand.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDebug>

ListFilesInFolderCommand::ListFilesInFolderCommand(const QString& folderPath,
                                                   const QString& pattern,
                                                   const QString& descr)
{
    m_params["folderPath"] = folderPath;
    if (!pattern.trimmed().isEmpty())
        m_params["pattern"] = pattern;
    m_description = descr.isEmpty()
        ? QString("Lists files in a directory with line counts. "
                  "Example: { \"command\": \"LIST_FILES_IN_FOLDER\", \"params\": { \"folderPath\": \"src/ui\" } }")
        : descr;
}

bool ListFilesInFolderCommand::apply(DevAssManager* devAss, QString* err)
{
    QString folderPath = m_params["folderPath"].toString();
    if (folderPath.trimmed().isEmpty()) {
        if (err) *err = "LIST_FILES_IN_FOLDER: folderPath is empty";
        return false;
    }
    
    // Normalize path relative to project root
    if (devAss && !QFileInfo(folderPath).isAbsolute()) {
        folderPath = QDir(devAss->projectRoot().absolutePath()).filePath(folderPath);
        // Update path in params for reporting
        m_params["folderPath"] = folderPath;
    }

    QDir dir(folderPath);
    if (!dir.exists()) {
        if (err) *err = QString("Folder does not exist: %1").arg(folderPath);
        return false;
    }

    // Get list of files (non-recursive)
    const QString pattern = m_params.value("pattern").toString().trimmed();
    if (!pattern.isEmpty()) {
        dir.setNameFilters(QStringList{pattern});
    }
    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files, QDir::Name);
    
    if (fileInfoList.isEmpty()) {
        if (err) *err = QString("Folder has no files: %1").arg(folderPath);
        return false;
    }

    QStringList fileResults;
    for (const QFileInfo& fileInfo : fileInfoList) {
        QString filePath = fileInfo.absoluteFilePath();
        QFile file(filePath);
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            int lineCount = 0;
            while (!stream.atEnd()) {
                stream.readLine();
                ++lineCount;
            }
            
            fileResults << QString("%1 (%2 lines)").arg(fileInfo.fileName()).arg(lineCount);
            file.close();
        } else {
            fileResults << QString("%1 (unable to open)").arg(fileInfo.fileName());
        }
    }

    // Store result
    m_result = fileResults.join("\n");

    return true;
}

QJsonObject ListFilesInFolderCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString ListFilesInFolderCommand::getResult() const
{
    QString result;
    if (wasSuccessful()) {
        result = QString("LIST_FILES_IN_FOLDER: %1\n[status]\nFile listing completed\nexitCode=0\n[files]\n%2")
               .arg(m_params["folderPath"].toString()).arg(m_result);
    } else {
        result = QString("LIST_FILES_IN_FOLDER: %1\n[status]\nFile listing failed\nexitCode=1")
               .arg(m_params["folderPath"].toString());
    }
    
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}
