#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

class Command;

// What the app allows a model-proposed command to do, decided before anything runs.
//
// Every command gets a risk level. The confirmation dialog and the "auto-approve read-only"
// mode use it, and dangerous commands are never pre-selected for the user. Before a batch
// runs, each command is also checked against the current prompt step's allowed commands and
// against the project folder: a path parameter that leads outside the open project rejects
// the command.
namespace CommandPolicy {

enum class Risk {
    ReadOnly,   // reads the project or app state; nothing on disk changes
    Modifying,  // changes project files or app state; file changes can be undone
    Dangerous   // runs programs, deletes, or reaches outside the project
};

Risk riskOf(const QString& commandName);
QString riskLabel(Risk risk);

// True if every command in the batch is read-only (an empty batch is not).
bool isReadOnlyBatch(const QList<Command*>& commands);

// Parameter names that hold file or folder paths.
const QStringList& pathParamNames();

// Checks the command's path parameters against the project folder and, when `allowed` is
// not empty, a modifying or dangerous command's name against it (read-only commands are
// always allowed). Returns false with a reason in `errorOut`.
bool check(const QString& name, const QJsonObject& params, const QString& projectRoot,
           const QStringList& allowed, QString* errorOut);
bool check(const Command* command, const QString& projectRoot,
           const QStringList& allowed, QString* errorOut);

// Short human-readable description of exactly what the command will do: the command line
// for a shell command, the URL for OPEN_URL, the paths and line ranges for file commands.
// Built from the parameters, never from the model's own "description" text.
QString details(const QString& name, const QJsonObject& params, const QString& projectRoot);

} // namespace CommandPolicy
