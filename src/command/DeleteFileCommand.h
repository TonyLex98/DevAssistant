// DeleteFileCommand.h
#pragma once
#include "Command.h"

class DeleteFileCommand : public Command
{
public:
    DeleteFileCommand(const QString& path,
                      const QString& descr = QString());

    QString name() const override { return "DELETE_FILE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;

private:
    QJsonObject m_params;
    QString m_description;
};
