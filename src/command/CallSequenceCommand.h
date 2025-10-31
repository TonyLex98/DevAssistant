#pragma once
#include "Command.h"

class CallSequenceCommand : public Command
{
public:
    CallSequenceCommand(const QString& sequenceName = QString(), bool needSummary = true, const QString& description = QString()) {
        m_params["sequenceName"] = sequenceName;
        m_params["summarize"] = needSummary;
        m_description = description.isEmpty() ? QString("Вызвать другую последовательность: %1").arg(sequenceName) : description;
    }
    
    QString name() const override { return "CALL_SEQUENCE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    
    bool apply(DevAssManager* devAss, QString* err) override { 
        Q_UNUSED(devAss); 
        Q_UNUSED(err); 
        // This command is handled specially in MainWindow, not through normal apply
        return true; 
    }
    
    QJsonObject toJson() const override { 
        QJsonObject o; 
        o["command"] = name(); 
        o["params"] = m_params; 
        o["description"] = m_description; 
        return o; 
    }
    
    QString sequenceName() const { 
        return m_params.value("sequenceName").toString(); 
    }
    
    bool needSummary() const {
        if (m_params.contains("summarize")) {
            return m_params.value("summarize").toBool();
        }
        return true; // Default to true
    }

private:
    QJsonObject m_params;
    QString m_description;
};