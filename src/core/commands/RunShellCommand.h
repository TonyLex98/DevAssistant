// RunShellCommand.h
#pragma once
#include "core/commands/Command.h"
#include <QProcess>

class RunShellCommand : public Command
{
public:
    /* command: an arbitrary shell command. */
    RunShellCommand(const QString& command = QString(),
                    const QString& cwd = QString(),
                    int timeoutMs = 120000,
                    const QString& descr = QString());

    QString name() const override { return "RUN_SHELL"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    bool runsInBackground() const override { return true; }  // waits on a process, the network or the disk

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
