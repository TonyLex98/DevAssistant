// CopyFileCommand.h
#pragma once
#include "core/commands/Command.h"

class CopyFileCommand : public Command
{
public:
    CopyFileCommand(const QString& fromPath,
                    const QString& toPath,
                    bool overwrite = false,
                    const QString& descr = QString());

    QString name() const override { return "COPY_FILE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
    QString m_stdout;
};

