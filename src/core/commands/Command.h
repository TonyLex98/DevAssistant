// Command.h
#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QDateTime>
#include "core/project/DevAssManager.h"

class DevAssManager;   // forward
class ModelAdapter;    // forward - LLM adapter interface

class Command
{
public:
    virtual ~Command() = default;

    /* command name, e.g. "CREATE_FILE" */
    virtual QString name() const = 0;

    /* command parameters, usable as is */
    virtual QJsonObject params() const = 0;

    /* short description of what the command does */
    virtual QString description() const = 0;

    /* apply the command to the file system.
       `devAss` - description manager, needed to update .dapro.
       `errorOut` - error message (if false). */
    virtual bool apply(DevAssManager* devAss,
                       QString* errorOut = nullptr) = 0;

    /* apply command using LLM (optional).
       This method can be overridden by commands that require LLM.
       `devAss` - description manager.
       `adapter` - ModelAdapter for LLM-based generation.
       `errorOut` - error message (if false). */
    virtual bool applyWithLLM(DevAssManager* devAss,
                              ModelAdapter* adapter,
                              QString* errorOut = nullptr) {
        Q_UNUSED(adapter)
        return apply(devAss, errorOut);
    }

    /* serialize to JSON (also usable for *.dadescr) */
    virtual QJsonObject toJson() const = 0;

    /* command result (shown to the user and passed to the LLM) */
    virtual QString getResult() const { return QString(); }

    /* execution status */
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
