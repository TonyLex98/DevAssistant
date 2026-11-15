#include "ui/controllers/DialogAgentHost.h"
#include <QCoreApplication>
#include <QInputDialog>
#include "ui/dialogs/CommandConfirmDialog.h"

void DialogAgentHost::confirmCommands(const QList<Command*>& commands, const QString& projectRoot,
                                      std::function<void(const QList<Command*>&)> done)
{
    CommandConfirmDialog dialog(commands, projectRoot, m_parent);
    done(dialog.exec() == QDialog::Accepted ? dialog.confirmed() : QList<Command*>());
}

void DialogAgentHost::askUser(const QString& question, std::function<void(bool, const QString&)> done)
{
    bool ok = false;
    const QString answer = QInputDialog::getText(
            m_parent, QCoreApplication::translate("DialogAgentHost", "Answer the model's question"),
            question, QLineEdit::Normal, QString(), &ok);
    done(ok, answer);
}
