// RunShellCommand.h
#pragma once
#include "Command.h"
#include <QProcess>

class RunShellCommand : public Command
{
public:
    /* command – произвольная консольная команда. */
    RunShellCommand(const QString& command = QString(),
                    const QString& descr = QString());

    QString name() const override { return "RUN_SHELL"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;

    QString stdoutText() const { return m_stdout; }
    QString stderrText() const { return m_stderr; }
    int exitCode() const { return m_exitCode; }

private:
    QJsonObject m_params;
    QString m_description;
    QString m_stdout;
    QString m_stderr;
    int m_exitCode = 0;
};
