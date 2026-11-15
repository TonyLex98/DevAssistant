// GitRestoreFileCommand.h
#pragma once
#include "core/commands/Command.h"

class GitRestoreFileCommand : public Command
{
public:
    GitRestoreFileCommand(const QString& path,
                          bool staged = false,
                          bool worktree = true,
                          const QString& descr = QString());

    QString name() const override { return "GIT_RESTORE_FILE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    bool runGit(const QStringList& args, DevAssManager* devAss, QString* err);

    QJsonObject m_params;
    QString m_description;
    QString m_stdout;
    QString m_stderr;
    int m_exitCode = 0;
};

