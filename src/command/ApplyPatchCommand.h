// ApplyPatchCommand.h
#pragma once
#include "Command.h"

class ApplyPatchCommand : public Command
{
public:
    ApplyPatchCommand(const QByteArray& patch,
                      int strip = 0,
                      const QString& descr = QString());

    QString name() const override { return "APPLY_PATCH"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
    QString m_summary;
};

