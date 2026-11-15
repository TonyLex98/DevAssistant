// GitDiffCommand.h
#pragma once
#include "core/commands/Command.h"

class GitDiffCommand : public Command
{
public:
    GitDiffCommand(bool staged = false,
                   int contextLines = -1,
                   const QString& path = QString(),
                   const QString& descr = QString());

    QString name() const override { return "GIT_DIFF"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    bool runsInBackground() const override { return true; }  // waits on a process, the network or the disk
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
    QString m_stdout;
    QString m_stderr;
    int m_exitCode = 0;
};

