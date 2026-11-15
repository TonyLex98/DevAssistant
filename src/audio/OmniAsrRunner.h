#pragma once
#include <QObject>
#include <QAudioInput>
#include <QFile>
#include <QProcess>
#include "audio/WavWriter.h"

class OmniAsrRunner : public QObject {
    Q_OBJECT
public:
    explicit OmniAsrRunner(QObject* parent = nullptr);
    ~OmniAsrRunner();

public slots:
    void startRecording();
    void stopRecording();

private:
    void checkAndInstallDependencies();

signals:
    void recognitionFinished(const QString& text);
    void errorOccurred(const QString& msg);
    void recordingStarted();
    void recordingStopped();

private slots:
    void onAudioReady();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QAudioInput* m_audioInput;
    QIODevice* m_audioDevice;
    WavWriter m_wavWriter;
    QString m_tempFilePath;
    QProcess* m_process;
    QAudioFormat m_format;
};
