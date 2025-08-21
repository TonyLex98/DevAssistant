// OpenFileContextCommand.h
#pragma once
#include "Command.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

class OpenFileContextCommand : public Command
{
public:
    OpenFileContextCommand(const QString& path,
                           const QString& descr = QString())
        : m_path(path)
    {
        m_description = descr.isEmpty()
            ? QString("Открыть файл «%1» и добавить в контекст если не хватает данных").arg(path)
            : descr;
    }

    QString name() const override { return "OPEN_FILE"; }
    QJsonObject params() const override
    {
        QJsonObject o;
        o["path"] = m_path;
        return o;
    }
    QString description() const override { return m_description; }

    bool apply(DevAssManager*, QString* err) override
    {
        QFile f(m_path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (err) *err = QString("Не удалось открыть файл %1").arg(m_path);
            return false;
        }

        m_content = QString::fromUtf8(f.readAll());
        f.close();
        return true;
    }

    /* После применения можно получить содержимое файла */
    const QString& content() const { return m_content; }

    const QString& path() const { return m_path; }

    QJsonObject toJson() const override
    {
        QJsonObject obj;
        obj["command"] = name();
        obj["params"] = params();
        obj["description"] = description();
        return obj;
    }

private:
    QString m_path;
    QString m_description;
    QString m_content;   // хранит прочитанное содержимое
};
