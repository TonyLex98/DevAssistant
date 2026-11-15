// ReplaceInFileCommand.h
#pragma once
#include "core/commands/Command.h"

class ReplaceInFileCommand : public Command
{
public:
    ReplaceInFileCommand(const QString& path, const QByteArray& content,
                         const QString& descr = QString());

    QString name() const override { return "REPLACE_IN_FILE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
};
