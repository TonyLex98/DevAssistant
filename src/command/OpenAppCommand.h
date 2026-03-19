#pragma once
#include "Command.h"
#include <QJsonValue>

class OpenAppCommand : public Command
{
public:
    OpenAppCommand(const QString& app = QString(),
                   const QJsonValue& args = QJsonValue(),
                   const QString& cwd = QString(),
                   bool detached = true,
                   int waitMs = 15000,
                   const QString& descr = QString());

    QString name() const override { return "OPEN_APP"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
    bool m_started = false;
    bool m_finished = false;
    bool m_detachedLaunch = true;
    int m_exitCode = 0;
    qint64 m_pid = 0;
};
