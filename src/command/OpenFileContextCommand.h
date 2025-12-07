// OpenFileContextCommand.h
#pragma once
#include "Command.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

class OpenFileContextCommand : public Command
{
public:
    OpenFileContextCommand(const QString& path,
                           const QString& descr = QString())
        : m_path(path)
    {
        m_description = descr.isEmpty()
            ? QString("Reads a file and returns its content with line numbers. Use this to examine a file before editing. "
                      "Output format: '<line_number>: <content>'. "
                      "Example: { \"command\": \"OPEN_FILE\", \"params\": { \"path\": \"src/main.cpp\" } }")
            : descr;
    }

    QString name() const override { return "OPEN_FILE"; }
    QJsonObject params() const override
    {
        QJsonObject o;
        o["path"] = m_path;
        return o;
    }
    QString description() const override { return m_description; }

    bool apply(DevAssManager* devAss, QString* err) override
    {
        QString path = m_path;
        if (path.trimmed().isEmpty()) {
            if (err) *err = "OPEN_FILE: path is empty";
            return false;
        }
        // Normalize path to project root if needed
        if (devAss && !QFileInfo(path).isAbsolute()) {
            path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
            m_path = path;
        }
        
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (err) *err = QString("Failed to open file %1").arg(path);
            return false;
        }

        QTextStream in(&f);
        QStringList lines;
        int lineNum = 1;
        while (!in.atEnd()) {
            lines << QString("%1: %2").arg(lineNum++).arg(in.readLine());
        }
        m_content = lines.join("\n");
        f.close();
        return true;
    }

    /* After apply() call get the file content */
    const QString& content() const { return m_content; }

    const QString& path() const { return m_path; }

    QJsonObject toJson() const override
    {
        QJsonObject obj;
        obj["command"] = name();
        obj["params"] = params();
        obj["description"] = description();
        return obj;
    }

    QString getResult() const override {
        QString result = QString("OPEN_FILE: %1\n[status]\n%2\nexitCode=%3")
                .arg(m_path)
                .arg(wasSuccessful() ? "File loaded into context" : "Failed to open file")
                .arg(wasSuccessful() ? 0 : 1);
        if (!getErrorMessage().isEmpty()) {
            result += QString("\n[error]\n%1").arg(getErrorMessage());
        }
        return result;
    }

private:
    QString m_path;
    QString m_description;
    QString m_content;   // stored content
};
