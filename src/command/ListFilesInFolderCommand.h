#pragma once
#include "Command.h"

class ListFilesInFolderCommand : public Command
{
public:
    ListFilesInFolderCommand(const QString& folderPath,
                             const QString& descr = QString());

    QString name() const override { return "LIST_FILES_IN_FOLDER"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;

    QString folderPath() const { return m_params["folderPath"].toString(); }
    
    QString getResult() const override;

private:
    QJsonObject m_params;
    QString m_description;
    QString m_result;  // Для хранения результата выполнения
};