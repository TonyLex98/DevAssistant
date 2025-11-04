#pragma once
#include <QDialog>
#include "../ui/customWidgets/StyledListWidget.h"
#include <StyleTextEdit.h>
#include "../core/ChangeApplier.h"

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
