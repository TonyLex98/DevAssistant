#pragma once
#include <QWidget>
#include <QLabel>
#include "../ui/customWidgets/MemoryLineEdit.h"
#include "../ui/customWidgets/StyledButton.h"

class QTextEdit;
class QCheckBox;
class QSpinBox;
class VoskRunner;

class VoskWidget : public QWidget {
    Q_OBJECT
public:
    explicit VoskWidget(QWidget* parent=nullptr);
private slots:
    void onStart();
    void onStop();
    void onPartial(const QString& s);
    void onFinal(const QString& s);
    void onError(const QString& s);
signals:
    void voiceRecognized(const QString&);
private:
    MemoryLineEdit* m_dllEdit;
    MemoryLineEdit* m_modelEdit;
    QSpinBox*  m_srSpin;
    QCheckBox* m_wordsChk;
    QCheckBox* m_partialWordsChk;
    StyledButton* m_startBtn;
    StyledButton* m_stopBtn;
    QLabel*     m_partialLbl;
    QTextEdit*  m_log;

    VoskRunner* m_runner;
};