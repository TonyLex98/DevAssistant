// PromptSequenceEditor.h
#pragma once
#include <QWidget>
#include "PromptSequence.h"

class QListWidget;
class QPushButton;

class PromptSequenceEditor : public QWidget
{
    Q_OBJECT
public:
    explicit PromptSequenceEditor(QWidget *parent = nullptr);

    void setSequence(const PromptSequence &seq);
    PromptSequence sequence() const { return m_seq; }

signals:
    void sequenceChanged(const PromptSequence &seq); // чтобы MainWindow мог сохранить

private slots:
    void onAddStep();
    void onEditStep();
    void onRemoveStep();
    void onMoveUp();
    void onMoveDown();

private:
    void refreshList();

    PromptSequence   m_seq;
    QListWidget     *m_list = nullptr;
    QPushButton     *m_addBtn = nullptr,
                    *m_editBtn = nullptr,
                    *m_delBtn = nullptr,
                    *m_upBtn = nullptr,
                    *m_downBtn = nullptr;
};
