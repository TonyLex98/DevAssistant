// OpenFileRegionCommand.h
#pragma once
#include "Command.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDir>

class OpenFileRegionCommand : public Command
{
public:
    OpenFileRegionCommand(const QString& path,
                          const QString& regionExpr = QString(),
                          int startLine = -1, int endLine = -1,
                          const QString& descr = QString())
    {
        m_params["path"] = path;
        if (!regionExpr.isEmpty()) m_params["region"] = regionExpr;
        if (startLine>=1 && endLine>=startLine){ m_params["startLine"]=startLine; m_params["endLine"]=endLine; }
        
        if (descr.isEmpty()) {
            if (startLine >= 1 && endLine >= startLine) {
                m_description = QString("Open file lines %1-%2 into context").arg(startLine).arg(endLine);
            } else if (!regionExpr.isEmpty()) {
                m_description = QString("Open file region by regex");
            } else {
                m_description = QString("Open a specific region of a file");
            }
        } else {
            m_description = descr;
        }
    }

    QString name() const override { return "OPEN_FILE_REGION"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    bool apply(DevAssManager* devAss, QString* err) override {
        QString path = m_params.value("path").toString();
        if (path.trimmed().isEmpty()) {
            if (err) *err = "OPEN_FILE_REGION: path is empty";
            return false;
        }
        // Normalize path relative to project root
        if (devAss && !QFileInfo(path).isAbsolute()) {
            path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
            m_params["path"] = path;
        }
        const int startLine = m_params.value("startLine").toInt(-1);
        const int endLine = m_params.value("endLine").toInt(-1);
        if ((startLine > 0 || endLine > 0) && (startLine <= 0 || endLine <= 0 || endLine < startLine)) {
            if (err) *err = "OPEN_FILE_REGION: invalid start/end line range";
            return false;
        }
        return true;
    }

    QJsonObject toJson() const override {
        QJsonObject obj; obj["command"]=name(); obj["params"]=m_params; obj["description"]=m_description; return obj;
    }

    QString getResult() const override {
        QString rangeInfo;
        if (m_params.contains("startLine") && m_params.contains("endLine")) {
            rangeInfo = QString("lines %1-%2").arg(m_params.value("startLine").toInt()).arg(m_params.value("endLine").toInt());
        } else if (m_params.contains("region")) {
            rangeInfo = QString("region /%1/").arg(m_params.value("region").toString());
        } else {
            rangeInfo = "unspecified region";
        }

        QString result = QString("OPEN_FILE_REGION: %1 (%2)\n[status]\n%3\nexitCode=%4")
                .arg(m_params.value("path").toString(), rangeInfo)
                .arg(wasSuccessful() ? "Region added to context" : "Failed to add region")
                .arg(wasSuccessful() ? 0 : 1);
        if (!getErrorMessage().isEmpty()) {
            result += QString("\n[error]\n%1").arg(getErrorMessage());
        }
        return result;
    }

private:
    QJsonObject m_params;
    QString m_description;
};
