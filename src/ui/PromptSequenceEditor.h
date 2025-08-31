#pragma once
#include <QWidget>
#include "PromptSequence.h"

class QListWidget;
class StyledButton;
class QLineEdit;
class StyleTextEdit;

class PromptSequenceEditor : public QWidget
{
    Q_OBJECT
public:
    explicit PromptSequenceEditor(QWidget *parent = nullptr);

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

private:
    void refreshList();

    PromptSequence m_seq;                     // объект‑заглушка
    QListWidget     *m_list   = nullptr;
    StyledButton     *m_addBtn  = nullptr,
                    *m_editBtn = nullptr,
                    *m_delBtn  = nullptr,
                    *m_upBtn   = nullptr,
                    *m_downBtn = nullptr;

    /* Новые виджеты */
    QLineEdit      *m_nameEdit  = nullptr;
    StyleTextEdit      *m_descEdit  = nullptr;
};
