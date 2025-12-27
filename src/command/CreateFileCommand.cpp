// CreateFileCommand.cpp
#include "CreateFileCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDateTime>

CreateFileCommand::CreateFileCommand(const QString& path,
                                     const QByteArray& content,
                                     const QString& descr)
{
    m_params["path"]    = path;
    m_params["content"] = QString::fromUtf8(content);
    m_description = descr.isEmpty()
        ? QString("Creates a new file with the specified content. "
                  "Fails if the file already exists. "
                  "Example: { \"command\": \"CREATE_FILE\", \"params\": { \"path\": \"src/new_file.cpp\", \"content\": \"#include <iostream>\\n...\" } }")
        : descr;
}

bool CreateFileCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params["path"].toString();
    if (path.trimmed().isEmpty()) {
        if (err) *err = "CREATE_FILE: path is empty";
        return false;
    }
    
    // Normalize path relative to project root
    if (devAss && !QFileInfo(path).isAbsolute()) {
        path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
        // Update stored path for reporting
        m_params["path"] = path;
    }

    QByteArray content = m_params["content"].toString().toUtf8();

    if (devAss) {
        QList<Change> changes;
        changes << Change::createFile(path, content);
        return devAss->applyChanges(changes, err);
    }

    // Fallback: direct write if DevAssManager not available.
    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (err) *err = QString("Failed to create directory for file: %1").arg(dir.path());
        return false;
    }
    QFile f(path);
    if (f.exists()) {
        if (err) *err = QString("File already exists: %1").arg(path);
        return false;
    }
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QString("Failed to create file: %1").arg(path);
        return false;
    }
    f.write(content);
    f.close();
    return true;
}

QJsonObject CreateFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString CreateFileCommand::getResult() const
{
    QString result;
    if (wasSuccessful()) {
        result = QString("CREATE_FILE: %1\n[status]\nFile created successfully\nexitCode=0")
               .arg(m_params["path"].toString());
    } else {
        result = QString("CREATE_FILE: %1\n[status]\nFile creation failed\nexitCode=1")
               .arg(m_params["path"].toString());
    }
    
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}
