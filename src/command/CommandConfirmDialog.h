#pragma once
#include <QDialog>
#include <QList>
#include <QString>

#include "Command.h"

class QTreeWidget;

// Asks the user which of the model's commands to run.
//
// Each row shows what the command will actually do, built from its parameters (the command
// line, the paths, the line range), and its risk level. The model's own description is only
// shown as a tooltip, because a model could describe a destructive command as harmless.
// Dangerous commands are never pre-selected.
class CommandConfirmDialog : public QDialog
{
    Q_OBJECT
public:
    CommandConfirmDialog(const QList<Command*>& commands,
                         const QString& projectRoot = QString(),
                         QWidget* parent = nullptr);

    // The commands the user left checked, in their original order.
    QList<Command*> confirmed() const { return m_confirmed; }

private:
    void collectConfirmed();

    QList<Command*> m_commands;
    QList<Command*> m_confirmed;
    QTreeWidget* m_list = nullptr;
};
