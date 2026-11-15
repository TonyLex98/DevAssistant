// DeleteDirCommand.h
#pragma once
#include "core/commands/Command.h"

class DeleteDirCommand : public Command
{
public:
    DeleteDirCommand(const QString& path,
                     bool recursive = false,
                     const QString& descr = QString());

    QString name() const override { return "DELETE_DIR"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
};

