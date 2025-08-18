#pragma once
#include <QDialog>
#include <QListWidget>
#include <QTextEdit>
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
    QListWidget* m_list=nullptr;
    QTextEdit* m_preview=nullptr;
};
