#pragma once
#include "Command.h"

class ShowResultCommand : public Command
{
public:
    ShowResultCommand(const QString& content = QString(),
                      const QString& title = QString(),
                      const QString& descr = QString());

    QString name() const override { return "SHOW_RESULT"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

    QString title() const { return m_params.value("title").toString(); }
    QString content() const { return m_params.value("content").toString(); }

private:
    QJsonObject m_params;
    QString m_description;
};
