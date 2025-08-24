#pragma once
#include <QWidget>
#include <QLabel>

class QLineEdit;
class QPushButton;
class QLabel;
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
    QLineEdit* m_dllEdit;
    QLineEdit* m_modelEdit;
    QSpinBox*  m_srSpin;
    QCheckBox* m_wordsChk;
    QCheckBox* m_partialWordsChk;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QLabel*     m_partialLbl;
    QTextEdit*  m_log;

    VoskRunner* m_runner;
};