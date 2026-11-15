#pragma once
#include "core/commands/Command.h"

class ReasoningCommand : public Command
{
public:
    ReasoningCommand(const QString& reasoning = QString(), const QString& nextCommand = QString(), const QJsonObject& nextCommandParams = QJsonObject(), const QString& descr = QString()){
        m_params["reasoning"] = reasoning;
        m_params["nextCommand"] = nextCommand;
        m_params["nextCommandParams"] = nextCommandParams;
        m_description = descr.isEmpty()
            ? QString("Logs a reasoning step or thought process. "
                      "Use this to plan complex actions or explain your logic before executing commands. "
                      "Example: { \"command\": \"REASONING\", \"params\": { \"reasoning\": \"I need to check the file content first to see where to add the new function.\" } }")
            : descr;
    }
    
    QString name() const override { return "REASONING"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    
    bool apply(DevAssManager* devAss, QString* err) override { 
        Q_UNUSED(devAss); 
        Q_UNUSED(err); 
        return true; 
    }
    
    QJsonObject toJson() const override { 
        QJsonObject o; 
        o["command"] = name(); 
        o["params"] = m_params; 
        o["description"] = m_description; 
        return o; 
    }
    
    QString getResult() const override { 
        // Return the reasoning as the result - this will be added to the context
        return m_params["reasoning"].toString(); 
    }

private:
    QJsonObject m_params;
    QString m_description;
};
