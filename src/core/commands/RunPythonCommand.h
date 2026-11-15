// RunPythonCommand.h
#pragma once
#include "core/commands/Command.h"
#include <QProcess>

class RunPythonCommand : public Command
{
public:
    /* scriptPath: path to a script file.
       inlineCode: code to run via `-c`. If both are empty, the command is invalid.
    */
    RunPythonCommand(const QString& scriptPath = QString(),
                     const QByteArray& inlineCode = QByteArray(),
                     const QString& cwd = QString(),
                     int timeoutMs = 120000,
                     const QString& descr = QString());

    QString name() const override { return "RUN_PYTHON"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    /* Runs `python` with either the script file or the code via `-c`. */
    bool apply(DevAssManager* devAss, QString* err) override;

    QJsonObject toJson() const override;

    QString stdoutText() const { return m_stdout; }
    QString stderrText() const { return m_stderr; }
    int exitCode() const { return m_exitCode; }
    
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
    QString m_stdout;
    QString m_stderr;
    int m_exitCode = 0;
};
