// CommandConfirmDialog.cpp (модифицирован)
#include "CommandConfirmDialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QCheckBox>

CommandConfirmDialog::CommandConfirmDialog(const QList<Command*>& cmds,
                                          QWidget* parent)
    : QDialog(parent), m_commands(cmds)
{
    setWindowTitle("Подтверждение команд");
    resize(500, 400);
    QVBoxLayout* v = new QVBoxLayout(this);

    QListWidget* list = new QListWidget(this);
    for (Command* c : m_commands) {
        QListWidgetItem* item = new QListWidgetItem(c->description(), list);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, QVariant::fromValue<void*>(c));
    }
    v->addWidget(list);

    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok
                                               | QDialogButtonBox::Cancel,
                                               this);
    v->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted,
            this, &CommandConfirmDialog::onAccept);
    connect(bb, &QDialogButtonBox::rejected,
            this, &CommandConfirmDialog::onReject);
}

void CommandConfirmDialog::onAccept()
{
    QListWidget* list = findChild<QListWidget*>();
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        if (item->checkState() == Qt::Checked) {
            Command* c = static_cast<Command*>(
                    item->data(Qt::UserRole).value<void*>());
            m_confirmed.append(c);
        }
    }
    accept();
}

void CommandConfirmDialog::onReject()
{
    reject();
}
