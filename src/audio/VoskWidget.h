#pragma once
#include <QWidget>
#include "../ui/customWidgets/MemoryLineEdit.h"
#include "../ui/customWidgets/StyledButton.h"
#include "../ui/customWidgets/StyledLabel.h"
#include "../ui/customWidgets/StyledTextEdit.h"
#include "../ui/customWidgets/StyledCheckBox.h"
#include "../ui/customWidgets/StyledSpinBox.h"

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
    StyledSpinBox*  m_srSpin;
    StyledCheckBox* m_wordsChk;
    StyledCheckBox* m_partialWordsChk;
    StyledButton* m_startBtn;
    StyledButton* m_stopBtn;
    StyledLabel*     m_partialLbl;
    StyledTextEdit*  m_log;

    VoskRunner* m_runner;
};