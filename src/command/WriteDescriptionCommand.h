// WriteDescriptionCommand.h
#pragma once
#include "Command.h"

class WriteDescriptionCommand : public Command
{
public:
    WriteDescriptionCommand(const QString& sourceFilePath,
                            const QString& description);

    QString name() const override { return "WRITE_DESCRIPTION"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;

private:
    QJsonObject m_params;   // { "sourceFilePath": "...", "description": "..." }
    QString m_description;  // для *.dadescr
};
