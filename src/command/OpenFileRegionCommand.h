// OpenFileRegionCommand.h
#pragma once
#include "Command.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDir>

class OpenFileRegionCommand : public Command
{
    QString m_content;

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
            m_description = "Reads a specific range of lines from a file. Returns content with line numbers. "
                            "Use this to read large files in chunks. "
                            "Example: { \"command\": \"OPEN_FILE_REGION\", \"params\": { \"path\": \"src/main.cpp\", \"startLine\": 10, \"endLine\": 50 } }";
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
        const QString regionExpr = m_params.value("region").toString();

        if (startLine == -1 && endLine == -1 && regionExpr.isEmpty()) {
             if (err) *err = "OPEN_FILE_REGION: specify startLine/endLine or region regex";
             return false;
        }

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (err) *err = QString("Failed to open file %1").arg(path);
            return false;
        }

        QTextStream in(&f);
        QStringList lines;
        int lineNum = 1;
        QRegularExpression regex(regionExpr);

        while (!in.atEnd()) {
            QString line = in.readLine();
            bool include = false;
            
            if (startLine != -1 && endLine != -1) {
                if (lineNum >= startLine && lineNum <= endLine) {
                    include = true;
                }
            } else if (!regionExpr.isEmpty()) {
                if (regex.match(line).hasMatch()) {
                    include = true;
                }
            }
            
            if (include) {
                 lines << QString("%1: %2").arg(lineNum).arg(line);
            }
            lineNum++;
        }
        m_content = lines.join("\n");
        f.close();
        return true;
    }

    const QString& content() const { return m_content; }

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

        QString result = QString("OPEN_FILE_REGION: %1 (%2)\n[status]\n%3\n[content]\n%4\nexitCode=%5")
                .arg(m_params.value("path").toString(), rangeInfo)
                .arg(wasSuccessful() ? "Region read successfully" : "Failed to read region")
                .arg(m_content)
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
