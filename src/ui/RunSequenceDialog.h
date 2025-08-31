// RunSequenceDialog.h
#pragma once
#include <QDialog>
#include <QString>
#include <PromptSequence.h>

class QListWidget;
class StyledButton;

class RunSequenceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RunSequenceDialog(const QMap<QString, PromptSequence> &seqs,
                               QWidget *parent = nullptr);
    QString selectedFilePath() const { return m_selectedPath; }

private slots:
    void onOk();

private:
    QListWidget *m_list = nullptr;
    StyledButton *m_okBtn = nullptr;
    QString      m_selectedPath;
};
