#pragma once
#include "Command.h"

class ClipboardSetCommand : public Command
{
public:
    ClipboardSetCommand(const QString& text = QString(), const QString& descr = QString());

    QString name() const override { return "CLIPBOARD_SET"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
    bool m_ok = false;
};

