#include "ui/controllers/ContextTreeView.h"
#include <QCoreApplication>
#include <QTreeWidget>
#include "core/context/ContextManager.h"

namespace {

void paint(QTreeWidgetItem* item, const QColor& background, const QColor& foreground, const QString& tooltip)
{
    for (int column = 0; column < 2; ++column) {
        item->setBackground(column, background);
        item->setForeground(column, foreground);
        if (!tooltip.isEmpty())
            item->setToolTip(column, tooltip);
    }
}

void addCommand(QTreeWidgetItem* item, const CommandContextInfo& info, const QString& label)
{
    item->setText(0, info.timestamp.toString(QStringLiteral("hh:mm:ss")));
    item->setText(1, label);
    item->setData(0, Qt::UserRole, info.id);
    switch (info.state) {
    case CommandState::Active:
        paint(item, QColor(200, 255, 200), Qt::darkGreen, QCoreApplication::translate("ContextTreeView", "Active in context"));
        break;
    case CommandState::Invalidated:
        paint(item, QColor(255, 200, 200), Qt::red, QCoreApplication::translate("ContextTreeView", "Invalidated: %1").arg(info.reason));
        break;
    case CommandState::Removed:
        paint(item, QColor(200, 200, 200), Qt::gray, QCoreApplication::translate("ContextTreeView", "Command was removed from context"));
        break;
    }
    if (info.result.isEmpty())
        return;
    auto* result = new QTreeWidgetItem(item);
    result->setText(0, QCoreApplication::translate("ContextTreeView", "Result:"));
    QStringList lines = info.result.split(QLatin1Char('\n'));
    if (lines.size() > 10)
        lines = lines.mid(0, 10) << QCoreApplication::translate("ContextTreeView", "... (click to expand)");
    result->setText(1, lines.join(QLatin1Char('\n')));
    result->setBackground(0, QColor(240, 240, 240));
    result->setBackground(1, QColor(245, 245, 245));
    result->setForeground(0, Qt::darkBlue);
    result->setForeground(1, Qt::darkBlue);
}

} // namespace

namespace ContextTreeView {

void populate(QTreeWidget* tree, const ContextManager& history)
{
    tree->clear();
    const QList<CommandSequenceInfo> sequences = history.getCommandSequences();
    if (sequences.isEmpty()) {
        for (const CommandContextInfo& info : history.getAllCommands()) {
            QString label = info.description;
            if (!info.sequenceId.isEmpty())
                label = QStringLiteral("[%1] %2").arg(history.getSequenceInfo(info.sequenceId).name, info.description);
            addCommand(new QTreeWidgetItem(tree), info, label);
        }
    } else {
        for (const CommandSequenceInfo& seq : sequences) {
            auto* seqItem = new QTreeWidgetItem(tree);
            seqItem->setText(0, seq.timestamp.toString(QStringLiteral("hh:mm:ss")));
            seqItem->setText(1, QCoreApplication::translate("ContextTreeView", "SEQUENCE: %1").arg(seq.name));
            seqItem->setData(0, Qt::UserRole, seq.id);
            paint(seqItem, QColor(255, 235, 205), Qt::darkBlue, seq.description);
            for (const CommandContextInfo& info : history.getCommandsInSequence(seq.id))
                addCommand(new QTreeWidgetItem(seqItem), info, info.description);
        }
    }
    // The structure open, the results folded.
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* top = tree->topLevelItem(i);
        top->setExpanded(true);
        for (int j = 0; j < top->childCount(); ++j)
            top->child(j)->setExpanded(false);
    }
}

} // namespace ContextTreeView
