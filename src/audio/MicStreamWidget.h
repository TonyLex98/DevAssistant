#pragma once
#include <QWidget>
#include <StreamRunner.h>
#include "../ui/customWidgets/MemoryLineEdit.h"
#include "../ui/customWidgets/StyledSpinBox.h"
#include "../ui/customWidgets/StyledButton.h"
#include "../ui/customWidgets/StyledTextEdit.h"

class StreamRunner;

class MicStreamWidget : public QWidget {
    Q_OBJECT
public:
    explicit MicStreamWidget(QWidget* parent=nullptr);
private slots:
    void onStart();
    void onStop();
    void onText(const QString& s);
    void onFinished(int, QProcess::ExitStatus);
    void onError(const QString& msg);
private:
    MemoryLineEdit*  m_exeEdit;
    MemoryLineEdit*  m_modelEdit;
    MemoryLineEdit*  m_langEdit;
    StyledSpinBox*   m_stepSpin;
    StyledSpinBox*   m_lenSpin;
    StyledSpinBox*   m_keepSpin;
    StyledSpinBox*   m_thrSpin;
    StyledButton* m_startBtn;
    StyledButton* m_stopBtn;
    StyledTextEdit*  m_log;

    StreamRunner* m_runner;
};
