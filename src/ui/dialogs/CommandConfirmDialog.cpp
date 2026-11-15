#include "ui/dialogs/CommandConfirmDialog.h"

#include "core/commands/CommandPolicy.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QColor riskColor(CommandPolicy::Risk risk)
{
    switch (risk) {
    case CommandPolicy::Risk::ReadOnly:  return QColor(QStringLiteral("#15803d"));
    case CommandPolicy::Risk::Modifying: return QColor(QStringLiteral("#b45309"));
    case CommandPolicy::Risk::Dangerous: return QColor(QStringLiteral("#b91c1c"));
    }
    return QColor();
}

} // namespace

CommandConfirmDialog::CommandConfirmDialog(const QList<Command*>& commands,
                                           const QString& projectRoot,
                                           QWidget* parent)
    : QDialog(parent), m_commands(commands)
{
    setWindowTitle(tr("Confirm commands"));
    resize(760, 420);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("The model wants to run these commands. Check the ones to run. "
                                "Dangerous commands are not selected by default."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_list = new QTreeWidget(this);
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({tr("Command"), tr("Risk"), tr("What it will do")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(false);
    m_list->setWordWrap(true);
    m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    for (Command* c : m_commands) {
        if (!c)
            continue;
        const CommandPolicy::Risk risk = CommandPolicy::riskOf(c->name());
        auto* item = new QTreeWidgetItem(m_list);
        item->setText(0, c->name());
        item->setText(1, CommandPolicy::riskLabel(risk));
        item->setForeground(1, riskColor(risk));
        item->setText(2, CommandPolicy::details(c->name(), c->params(), projectRoot));
        item->setFont(2, mono);
        const QString modelSays = c->description().trimmed();
        if (!modelSays.isEmpty())
            item->setToolTip(2, tr("Model's description: %1").arg(modelSays));
        item->setCheckState(0, risk == CommandPolicy::Risk::Dangerous ? Qt::Unchecked : Qt::Checked);
        item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(c));
    }
    layout->addWidget(m_list, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Run selected"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        collectConfirmed();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CommandConfirmDialog::collectConfirmed()
{
    m_confirmed.clear();
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_list->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            m_confirmed.append(static_cast<Command*>(item->data(0, Qt::UserRole).value<void*>()));
    }
}
