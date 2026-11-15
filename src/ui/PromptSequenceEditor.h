#pragma once
#include <QWidget>
#include "PromptSequence.h"
#include <QCheckBox>
#include <QListWidget>

class QListWidget;
class StyledButton;
class StyleTextEdit;
class QLineEdit;
class PromptStepDialog;

class PromptSequenceEditor : public QWidget
{
    Q_OBJECT
public:
    explicit PromptSequenceEditor(QWidget *parent = nullptr);
    void setSequenceManager(class PromptSequenceManager *seqMgr);  // Set sequence manager for reference selection
    void setCurrentSequencePath(const QString& sequencePath);  // Set path of current sequence for filtering sequences

    /* Load a sequence into the editor */
    void setSequence(const PromptSequence &seq);

    /* Current sequence, including name/description edits */
    PromptSequence sequence() const;

signals:
    void sequenceChanged(const PromptSequence &seq);   // lets MainWindow save the sequence

private slots:
    void onAddStep();
    void onEditStep();
    void onRemoveStep();
    void onMoveUp();
    void onMoveDown();

    /* Name/description field changes */
    void onNameChanged();
    void onDescriptionChanged();

private:
    void refreshList();
    void loadStepToEditor(int idx);
    void updateCurrentStepTitle();

    PromptSequence m_seq;                     // sequence being edited
    QListWidget     *m_list   = nullptr;
    StyledButton     *m_addBtn  = nullptr,
                    *m_editBtn = nullptr,
                    *m_delBtn  = nullptr,
                    *m_upBtn   = nullptr,
                    *m_downBtn = nullptr;

    QLineEdit          *m_nameEdit  = nullptr;
    StyleTextEdit      *m_descEdit  = nullptr;
    PromptStepDialog   *m_stepEditor = nullptr;
    
    PromptSequenceManager *m_seqMgr = nullptr; // To access all available sequences
    QString m_currentSequencePath; // Path to the currently edited sequence for filtering
    int m_currentStepIdx = -1;
};
