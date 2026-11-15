#pragma once
#include <QDialog>
#include "ui/widgets/StyledListWidget.h"
#include "ui/widgets/StyleTextEdit.h"
#include "core/project/ChangeApplier.h"

class ChangesDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChangesDialog(QWidget* parent=nullptr);
    void setChanges(const QList<Change>& ch);

private slots:
    void onItemChanged(int row);

private:
    QList<Change> m_changes;
    StyledListWidget* m_list=nullptr;
    StyleTextEdit* m_preview=nullptr;
};
