#pragma once
#include "Command.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDir>

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
        
        // Создаем более информативное описание, которое четко подскажет модели, какие параметры используются
        if (descr.isEmpty()) {
            if (startLine >= 1 && endLine >= startLine) {
                m_description = QString("Открыть конкретный участок файла (строки с %2 по %3); используется для чтения определенной части файла").arg(path).arg(startLine).arg(endLine);
            } else if (!regionExpr.isEmpty()) {
                m_description = QString("Открыть участок файла «%1» по регулярному выражению; используется для поиска и чтения участка кода по паттерну").arg(path).arg(regionExpr);
            } else {
                m_description = QString("Открыть участок файла по указанным строкам (startLine и endLine); используется для чтения определенной части файла").arg(path);
            }
        } else {
            m_description = descr;
        }
    }

    QString name() const override { return "OPEN_FILE_REGION"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    bool apply(DevAssManager* devAss, QString* err) override {
        QString path = m_params.value("path").toString();
        // Нормализуем путь: если путь относительный, делаем его относительно корневой директории проекта
        if (devAss && !QFileInfo(path).isAbsolute()) {
            path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
            // Обновляем путь в параметрах для отображения в результатах
            m_params["path"] = path;
        }
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
