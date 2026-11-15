#pragma once
#include <QList>
#include <QString>
#include <functional>

class Command;

// What the agent needs from the user while it works. The window implements it with dialogs;
// tests implement it with fixed answers. Each call may answer at once or later.
class AgentHost
{
public:
    virtual ~AgentHost() = default;

    // Shows the commands that change something and calls `done` with the ones the user
    // accepted; an empty list means the user declined.
    virtual void confirmCommands(const QList<Command*>& commands, const QString& projectRoot,
                                 std::function<void(const QList<Command*>& accepted)> done) = 0;

    // Asks the model's question (ASK_USER).
    virtual void askUser(const QString& question,
                         std::function<void(bool answered, const QString& answer)> done) = 0;
};
