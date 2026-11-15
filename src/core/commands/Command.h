#pragma once
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>
#include "core/project/DevAssManager.h"

// One action the model asks for: open a file, edit lines, run a program, search... The parser
// builds commands from the model's answer; the policy decides whether they may run; the agent
// runs them and gives their results back to the model.
class Command
{
public:
    virtual ~Command() = default;

    // The command's name, e.g. "CREATE_FILE".
    virtual QString name() const = 0;
    // Its parameters, as the model gave them (after normalization).
    virtual QJsonObject params() const = 0;
    // What it does, in a sentence.
    virtual QString description() const = 0;

    // Does the work. File changes go through `devAss`, which checks the paths and records them
    // for undo. On failure returns false and explains why in `errorOut`.
    virtual bool apply(DevAssManager* devAss, QString* errorOut = nullptr) = 0;

    // True for commands that wait on the network or on another program (searching the web,
    // running a shell command): the agent runs them in a worker thread, so the window stays
    // responsive. Such commands must not touch the file system through `devAss`.
    virtual bool runsInBackground() const { return false; }

    // The command as JSON (also used for the .dadescr files).
    virtual QJsonObject toJson() const = 0;

    // The result shown to the user and given to the model.
    virtual QString getResult() const { return QString(); }

    void setExecutionResult(bool success, const QString& error = QString())
    {
        m_executionSuccess = success;
        m_errorMessage = error;
    }
    bool wasSuccessful() const { return m_executionSuccess; }
    QString getErrorMessage() const { return m_errorMessage; }

private:
    bool m_executionSuccess = true;
    QString m_errorMessage;
};
