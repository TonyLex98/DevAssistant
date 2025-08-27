#pragma once
#include "Command.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

class OpenFileRegionCommand : public Command
{
public:
    OpenFileRegionCommand(const QString& path,
                          const QString& regionExpr = QString(),
                          int startLine = -1, int endLine = -1,
                          const QString& descr = QString())
    {
        m_params["path"] = path;
        if (!regionExpr.isEmpty()) m_params["region"] = regionExpr;
        if (startLine>=1 && endLine>=startLine){ m_params["startLine"]=startLine; m_params["endLine"]=endLine; }
        m_description = descr.isEmpty() ? QString("Открыть участок файла «%1»").arg(path) : descr;
    }

    QString name() const override { return "OPEN_FILE_REGION"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    // apply(): no filesystem change; returns true to indicate success
    bool apply(DevAssManager* devAss, QString* err) override {
        Q_UNUSED(devAss);
        Q_UNUSED(err);
        return true;
    }

    QJsonObject toJson() const override {
        QJsonObject obj; obj["command"]=name(); obj["params"]=m_params; obj["description"]=m_description; return obj;
    }

private:
    QJsonObject m_params;
    QString m_description;
};
