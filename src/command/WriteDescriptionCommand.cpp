// WriteDescriptionCommand.cpp
#include "WriteDescriptionCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDateTime>

WriteDescriptionCommand::WriteDescriptionCommand(const QString& sourceFilePath,
                                                const QString& descr)
    : m_description(descr.isEmpty() ? QString("Write/update descriptor for a source file") : descr)
{
    m_params["sourceFilePath"] = sourceFilePath;
    m_params["description"]    = descr;
}

bool WriteDescriptionCommand::apply(DevAssManager* devAss, QString* err)
{
    if (!devAss) {
        if (err) *err = "WRITE_DESCRIPTION: DevAssManager is not available";
        return false;
    }

    QString srcPath = m_params["sourceFilePath"].toString();
    if (srcPath.trimmed().isEmpty()) {
        if (err) *err = "WRITE_DESCRIPTION: sourceFilePath is empty";
        return false;
    }
    
    // Normalize path relative to project root when needed
    if (!QFileInfo(srcPath).isAbsolute()) {
        srcPath = QDir(devAss->projectRoot().absolutePath()).filePath(srcPath);
        m_params["sourceFilePath"] = srcPath;
    }
    
    if (!QFileInfo::exists(srcPath)) {
        if (err) *err = QString("WRITE_DESCRIPTION: source file not found: %1").arg(srcPath);
        return false;
    }

    const QString descr  = m_params["description"].toString();
    if (descr.isEmpty()) {
        if (err) *err = "WRITE_DESCRIPTION: description text is empty";
        return false;
    }

    // Build descriptor path relative to project root
    QString rel = QDir(devAss->projectRoot().absolutePath())
                      .relativeFilePath(srcPath);

    m_writtenDescriptorPath = devAss->devAssDir().filePath(rel + ".dadescr");

    // Ensure target directory exists
    QDir().mkpath(QFileInfo(m_writtenDescriptorPath).absolutePath());

    QFile f(m_writtenDescriptorPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QString("Failed to write descriptor %1").arg(m_writtenDescriptorPath);
        return false;
    }
    f.write(descr.toUtf8());
    f.close();

    /* update entry in .dapro */
    devAss->updateFileDescription(srcPath,
                                  QDateTime::currentDateTime(),
                                  true);
    devAss->save();

    return true;
}

QJsonObject WriteDescriptionCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString WriteDescriptionCommand::getResult() const
{
    const bool ok = wasSuccessful();
    QString result = QString("WRITE_DESCRIPTION: %1\n[status]\n%2\nexitCode=%3")
            .arg(m_params.value("sourceFilePath").toString())
            .arg(ok ? "Description file updated" : "Failed to write description")
            .arg(ok ? 0 : 1);

    if (!m_writtenDescriptorPath.isEmpty()) {
        result += QString("\n[descriptor]\n%1").arg(m_writtenDescriptorPath);
    }
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    return result;
}
