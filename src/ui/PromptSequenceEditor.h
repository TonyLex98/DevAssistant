#pragma once
#include <QWidget>
#include "PromptSequence.h"
#include <QCheckBox>
#include <QListWidget>

class QListWidget;
class StyledButton;
class QLineEdit;
class StyleTextEdit;

class PromptSequenceEditor : public QWidget
{
    Q_OBJECT
public:
    explicit PromptSequenceEditor(QWidget *parent = nullptr);
    void setSequenceManager(class PromptSequenceManager *seqMgr);  // Set sequence manager for reference selection

    /* Инициализируем редактор конкретной последовательностью */
    void setSequence(const PromptSequence &seq);

    /* Возвращаем текущую последовательность (с учётом изменений имени/описания) */
    PromptSequence sequence() const;

signals:
    void sequenceChanged(const PromptSequence &seq);   // сигнал, чтобы MainWindow мог сохранить

private slots:
    void onAddStep();
    void onEditStep();
    void onRemoveStep();
    void onMoveUp();
    void onMoveDown();

    /* Сигналы из полей имени/описания */
    void onNameChanged();
    void onDescriptionChanged();
    void onAddSequenceReference();  // New slot for adding sequence references

private:
    void refreshList();
    void updateSequenceReferenceList();  // Update the list of available sequence references

    PromptSequence m_seq;                     // объект‑заглушка
    QListWidget     *m_list   = nullptr;
    StyledButton     *m_addBtn  = nullptr,
                    *m_editBtn = nullptr,
                    *m_delBtn  = nullptr,
                    *m_upBtn   = nullptr,
                    *m_downBtn = nullptr;

    /* Новые виджеты */
    StyleTextEdit      *m_nameEdit  = nullptr;
    StyleTextEdit      *m_descEdit  = nullptr;
    
    /* Sequence reference widgets */
    StyledButton       *m_addRefBtn = nullptr;
    QListWidget        *m_refList = nullptr;  // List of selected sequence references
    PromptSequenceManager *m_seqMgr = nullptr; // To access all available sequences
};
