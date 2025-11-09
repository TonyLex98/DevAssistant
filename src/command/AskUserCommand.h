#pragma once
#include "Command.h"

class AskUserCommand : public Command
{
public:
    AskUserCommand(const QString& question = QString(), const QString& descr = QString()){
        m_params["question"] = question;
        m_description = descr.isEmpty() ? QString("Задать пользователю уточняющий вопрос; используется когда требуется дополнительная информация для выполнения задачи") : descr;
    }
    QString name() const override { return "ASK_USER"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override { Q_UNUSED(devAss); Q_UNUSED(err); return true; }
    QJsonObject toJson() const override { QJsonObject o; o["command"]=name(); o["params"]=m_params; o["description"]=m_description; return o; }
private:
    QJsonObject m_params;
    QString m_description;
};
