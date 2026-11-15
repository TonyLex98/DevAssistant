#pragma once
#include "core/workflow/AgentHost.h"

class QWidget;

// The agent's questions as dialogs: the command confirmation and ASK_USER.
class DialogAgentHost : public AgentHost
{
public:
    explicit DialogAgentHost(QWidget* parent) : m_parent(parent) {}

    void confirmCommands(const QList<Command*>& commands, const QString& projectRoot,
                         std::function<void(const QList<Command*>& accepted)> done) override;
    void askUser(const QString& question,
                 std::function<void(bool answered, const QString& answer)> done) override;

private:
    QWidget* m_parent;
};
