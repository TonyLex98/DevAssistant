#pragma once
#include <QWidget>
#include <StreamRunner.h>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QTextEdit;
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
    QLineEdit*  m_exeEdit;
    QLineEdit*  m_modelEdit;
    QLineEdit*  m_langEdit;
    QSpinBox*   m_stepSpin;
    QSpinBox*   m_lenSpin;
    QSpinBox*   m_keepSpin;
    QSpinBox*   m_thrSpin;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QTextEdit*  m_log;

    StreamRunner* m_runner;
};
