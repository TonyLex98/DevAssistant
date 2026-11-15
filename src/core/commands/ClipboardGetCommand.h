#pragma once
#include "core/commands/Command.h"

class ClipboardGetCommand : public Command
{
public:
    ClipboardGetCommand(int maxChars = 4000, const QString& descr = QString());

    QString name() const override { return "CLIPBOARD_GET"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
    QString m_value;
};

