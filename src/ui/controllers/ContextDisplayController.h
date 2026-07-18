#pragma once
#include <QObject>
#include "../../services/IService.h"

class ContextDisplayController : public QObject {
    Q_OBJECT
public:
    ContextDisplayController(IContextService* ctx, IUIUpdater* ui);

    void updateContextStatusDisplay();
    QString getContextChainText() const;
    QString getAllCommandsText() const;

private:
    void applyCommandStateStyles(QTreeWidgetItem* item, int state, const QString& reason);
    void addResultChild(QTreeWidgetItem* parent, const QString& result);

    IContextService* m_ctx;
    IUIUpdater* m_ui;
};
