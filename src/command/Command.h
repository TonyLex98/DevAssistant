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
    
    /* получить результат выполнения команды (для отображения пользователю и LLM) */
    virtual QString getResult() const { return QString(); }
    
    /* методы для установки и получения результата выполнения */
    void setExecutionResult(bool success, const QString& error = QString()) {
        m_executionSuccess = success;
        m_errorMessage = error;
    }
    
    bool wasSuccessful() const { return m_executionSuccess; }
    QString getErrorMessage() const { return m_errorMessage; }

private:
    bool m_executionSuccess = true;
    QString m_errorMessage;
};
