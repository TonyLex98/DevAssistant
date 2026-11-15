#pragma once
#include "core/commands/Command.h"

class CallSequenceCommand : public Command
{
public:
    CallSequenceCommand(const QString& sequenceName = QString(), bool needSummary = false, const QString& description = QString()) {
        m_params["sequenceName"] = sequenceName;
        m_params["summarize"] = needSummary;
        m_description = description.isEmpty() 
            ? QString("Calls another sequence of commands. "
                      "Example: { \"command\": \"CALL_SEQUENCE\", \"params\": { \"sequenceName\": \"deploy_app\" } }")
            : description;
    }
    
    QString name() const override { return "CALL_SEQUENCE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    
    bool apply(DevAssManager* devAss, QString* err) override { 
        Q_UNUSED(devAss); 
        if (m_params.value("sequenceName").toString().trimmed().isEmpty()) {
            if (err) *err = "CALL_SEQUENCE: sequenceName is empty";
            return false;
        }
        // The agent engine handles this command; apply() does nothing
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
        const bool ok = wasSuccessful();
        QString result = QString("CALL_SEQUENCE: %1\n[status]\n%2\nexitCode=%3")
                .arg(m_params.value("sequenceName").toString())
                .arg(ok ? "Sequence execution requested" : "Failed to request sequence")
                .arg(ok ? 0 : 1);
        if (!getErrorMessage().isEmpty()) {
            result += QString("\n[error]\n%1").arg(getErrorMessage());
        }
        return result;
    }
    
    QString sequenceName() const { 
        return m_params.value("sequenceName").toString(); 
    }
    
    bool needSummary() const {
        if (m_params.contains("summarize")) {
            return false;//m_params.value("summarize").toBool();
        }
        return true; // Default to true
    }

private:
    QJsonObject m_params;
    QString m_description;
};
