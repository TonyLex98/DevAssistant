// CreateFileCommand.h
#pragma once
#include "Command.h"

class CreateFileCommand : public Command
{
public:
    CreateFileCommand(const QString& path, const QByteArray& content,
                      const QString& descr = QString());

    QString name() const override { return "CREATE_FILE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;

    // Путь к файлу (может быть нормализован позже)
    QString path() const { return m_params["path"].toString(); }

private:
    QJsonObject m_params;
    QString m_description;
};
