
#pragma once
#include <QObject>
#include <QAudioFormat>
#include <QAudioInput> // Qt5; for Qt6 use QAudioSource
#include <QScopedPointer>
#include <QIODevice>
#include "audio/VoskDyn.h"

class VoskRunner : public QObject {
    Q_OBJECT
public:
    explicit VoskRunner(QObject* parent=nullptr);
    ~VoskRunner();

    void setDllPath(const QString& p)   { m_dllPath = p; }
    void setModelPath(const QString& p) { m_modelPath = p; }
    void setSampleRate(int sr)          { m_sampleRate = sr; }
    void setWords(bool on)              { m_words = on; }
    void setPartialWords(bool on)       { m_partialWords = on; }

public slots:
    bool start();
    void stop();

signals:
    void partialText(const QString& text);
    void finalText(const QString& text);
    void error(const QString& msg);

private slots:
    void onReadyRead();

private:
    bool initVosk();
    void freeVosk();
    void resetRecognizer();

    QString m_dllPath;
    QString m_modelPath;
    int m_sampleRate = 16000;
    bool m_words = false;
    bool m_partialWords = false;

    VoskDyn m_vosk;
    VoskModel* m_model = nullptr;
    VoskRecognizer* m_rec = nullptr;

    QAudioFormat m_fmt;
    QScopedPointer<QAudioInput> m_in; // Qt6: QAudioSource
    QIODevice* m_dev = nullptr;
};
