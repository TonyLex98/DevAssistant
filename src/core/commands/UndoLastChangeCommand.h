// UndoLastChangeCommand.h
#pragma once
#include "core/commands/Command.h"

class UndoLastChangeCommand : public Command
{
public:
    UndoLastChangeCommand(const QString& descr = QString());

    QString name() const override { return "UNDO_LAST_CHANGE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
};

