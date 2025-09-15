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

private:
    QJsonObject m_params;
    QString m_description;
};
