#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QList>

struct CommandParamDoc
{
    QString name;
    QString type;
    bool required = false;
    QString description;
};

struct CommandDoc
{
    QString name;
    QString summary;
    QList<CommandParamDoc> params;
    QStringList notes;
    QJsonObject example;
};

class CommandDocs
{
public:
    static CommandDoc doc(const QString& commandName);
    static QString renderForLLM(const CommandDoc& doc);

private:
    static QString compactJson(const QJsonObject& obj);
};

