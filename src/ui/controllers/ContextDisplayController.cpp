#include "ContextDisplayController.h"

ContextDisplayController::ContextDisplayController(IContextService* ctx, IUIUpdater* ui)
    : QObject(nullptr)
    , m_ctx(ctx)
    , m_ui(ui)
{
}

void ContextDisplayController::updateContextStatusDisplay()
{
    auto* list = m_ui->contextCommandsList();
    if (!list || !m_ctx->hasContextManager()) {
        return;
    }

    list->clear();

    auto allSequences = m_ctx->getCommandSequences();

    if (allSequences.isEmpty()) {
        auto allCommands = m_ctx->getAllCommands();

        for (const auto& cmdInfo : allCommands) {
            QTreeWidgetItem* commandItem = new QTreeWidgetItem(list);
            commandItem->setText(0, cmdInfo.timestamp.toString("hh:mm:ss"));

            QString desc = cmdInfo.description;
            if (!cmdInfo.sequenceId.isEmpty()) {
                auto seqInfo = m_ctx->getSequenceInfo(cmdInfo.sequenceId);
                desc = QString("[%1] %2").arg(seqInfo.name).arg(cmdInfo.description);
            }
            commandItem->setText(1, desc);

            applyCommandStateStyles(commandItem, cmdInfo.state, cmdInfo.reason);
            commandItem->setData(0, Qt::UserRole, cmdInfo.id);

            if (!cmdInfo.result.isEmpty()) {
                addResultChild(commandItem, cmdInfo.result);
            }
        }
    } else {
        for (const auto& seqInfo : allSequences) {
            QTreeWidgetItem* seqItem = new QTreeWidgetItem(list);
            seqItem->setText(0, seqInfo.timestamp.toString("hh:mm:ss"));
            seqItem->setText(1, QString("SEQUENCE: %1").arg(seqInfo.name));
            if (!seqInfo.description.isEmpty()) {
                seqItem->setToolTip(1, seqInfo.description);
            }

            seqItem->setBackground(0, QColor(255, 235, 205));
            seqItem->setBackground(1, QColor(255, 235, 205));
            seqItem->setForeground(0, Qt::darkBlue);
            seqItem->setForeground(1, Qt::darkBlue);
            seqItem->setData(0, Qt::UserRole, seqInfo.id);

            auto commandsInSequence = m_ctx->getCommandsInSequence(seqInfo.id);
            for (const auto& cmdInfo : commandsInSequence) {
                QTreeWidgetItem* commandItem = new QTreeWidgetItem(seqItem);
                commandItem->setText(0, cmdInfo.timestamp.toString("hh:mm:ss"));
                commandItem->setText(1, cmdInfo.description);

                applyCommandStateStyles(commandItem, cmdInfo.state, cmdInfo.reason);
                commandItem->setData(0, Qt::UserRole, cmdInfo.id);

                if (!cmdInfo.result.isEmpty()) {
                    addResultChild(commandItem, cmdInfo.result);
                }
            }
        }
    }
}

QString ContextDisplayController::getContextChainText() const
{
    return m_ctx->generateContextText();
}

QString ContextDisplayController::getAllCommandsText() const
{
    return m_ctx->getAllCommandsText();
}

// ─── private helpers ──────────────────────────────────────────────────

void ContextDisplayController::applyCommandStateStyles(QTreeWidgetItem* item, int state, const QString& reason)
{
    switch (state) {
        case 0: { // Active
            item->setBackground(0, QColor(200, 255, 200));
            item->setBackground(1, QColor(200, 255, 200));
            item->setForeground(0, Qt::darkGreen);
            item->setForeground(1, Qt::darkGreen);
            item->setToolTip(0, "Active in context");
            item->setToolTip(1, "Active in context");
            break;
        }
        case 1: { // Invalidated
            item->setBackground(0, QColor(255, 200, 200));
            item->setBackground(1, QColor(255, 200, 200));
            item->setForeground(0, Qt::red);
            item->setForeground(1, Qt::red);
            QString tooltip = QString("Invalidated: %1").arg(reason);
            item->setToolTip(0, tooltip);
            item->setToolTip(1, tooltip);
            break;
        }
        case 2: { // Removed
            item->setBackground(0, QColor(200, 200, 200));
            item->setBackground(1, QColor(200, 200, 200));
            item->setForeground(0, Qt::gray);
            item->setForeground(1, Qt::gray);
            item->setToolTip(0, "Command was removed from context");
            item->setToolTip(1, "Command was removed from context");
            break;
        }
    }
}

void ContextDisplayController::addResultChild(QTreeWidgetItem* parent, const QString& result)
{
    QTreeWidgetItem* resultItem = new QTreeWidgetItem(parent);
    resultItem->setText(0, "Result:");
    resultItem->setText(1, result);
    resultItem->setBackground(0, QColor(240, 240, 240));
    resultItem->setBackground(1, QColor(245, 245, 245));
    resultItem->setForeground(0, Qt::darkBlue);
    resultItem->setForeground(1, Qt::darkBlue);

    QStringList resultLines = result.split('\n');
    if (resultLines.size() > 10) {
        QStringList shortResult = resultLines.mid(0, 10);
        shortResult << "... (click to expand)";
        resultItem->setText(1, shortResult.join('\n'));
    } else {
        resultItem->setText(1, result);
    }
}
