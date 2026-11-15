// ContextShowCommand.h
#pragma once
#include "core/commands/Command.h"

class ContextShowCommand : public Command
{
public:
    ContextShowCommand(int maxChars = 8000, const QString& descr = QString());

    QString name() const override { return "CONTEXT_SHOW"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    // Handled by the agent engine; apply() does nothing.
    bool apply(DevAssManager* devAss, QString* err) override;

    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
};

