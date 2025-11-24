#pragma once
#include "Command.h"

class OpenSequenceCommand : public Command
{
public:
    OpenSequenceCommand(const QString& sequenceName = QString(), const QString& description = QString()) {
        m_params["sequenceName"] = sequenceName;
        m_description = description.isEmpty() ? QString("Открыть другую последовательность: %1").arg(sequenceName) : description;
    }
    
    QString name() const override { return "OPEN_SEQUENCE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    
    bool apply(DevAssManager* devAss, QString* err) override { 
        Q_UNUSED(devAss); 
        if (m_params.value("sequenceName").toString().trimmed().isEmpty()) {
            if (err) *err = "OPEN_SEQUENCE: sequenceName is empty";
            return false;
        }
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

    QString getResult() const override {
        const bool ok = wasSuccessful();
        QString result = QString("OPEN_SEQUENCE: %1\n[status]\n%2\nexitCode=%3")
                .arg(m_params.value("sequenceName").toString())
                .arg(ok ? "Sequence opened" : "Failed to open sequence")
                .arg(ok ? 0 : 1);
        if (!getErrorMessage().isEmpty()) {
            result += QString("\n[error]\n%1").arg(getErrorMessage());
        }
        return result;
    }

private:
    QJsonObject m_params;
    QString m_description;
};
