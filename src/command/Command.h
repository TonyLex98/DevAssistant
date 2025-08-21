// Command.h
#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QDateTime>
#include <DevAssManager.h>

class DevAssManager;   // forward

class Command
{
public:
    virtual ~Command() = default;

    /* имя команды, например "CREATE_FILE" */
    virtual QString name() const = 0;

    /* параметры команды – можно использовать как есть */
    virtual QJsonObject params() const = 0;

    /* описание (коротко) того, что делает команда */
    virtual QString description() const = 0;

    /* применить команду к файловой системе.
       `devAss` – менеджер описаний, нужен для обновления .dapro.
       `errorOut` – сообщение об ошибке (если false). */
    virtual bool apply(DevAssManager* devAss,
                       QString* errorOut = nullptr) = 0;

    /* сериализовать в JSON (можно использовать для *.dadescr) */
    virtual QJsonObject toJson() const = 0;
};
