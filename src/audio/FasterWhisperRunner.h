#pragma once
#include <QObject>
#include <QAudioInput>
#include <QProcess>
#include <QFile>
#include <QTimer>
#include <QVector>
#include "audio/WavWriter.h"

class FasterWhisperRunner : public QObject {
    Q_OBJECT
public:
    explicit FasterWhisperRunner(QObject* parent = nullptr);
    ~FasterWhisperRunner();

    bool isRecording() const { return m_isRecording; }

public slots:
    void startRecording();
    void stopRecording();

signals:
    void recognitionFinished(const QString& text);
    void errorOccurred(const QString& msg);
    void recordingStarted();
    void recordingStopped();
    void serverReady();
    void statusMessage(const QString& msg);
    void silenceDetected(); // New signal for silence detection

private slots:
    void onAudioReady();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessReadyRead();
    void processAudioChunk();
    void onSilenceTimeout();

private:
    void startServer();
    void stopServer();
    void checkAndInstallDependencies();
    double calculateRMS(const QByteArray& data);

    QAudioInput* m_audioInput;
    QIODevice* m_audioDevice;
    QProcess* m_process;
    QAudioFormat m_format;
    
    QByteArray m_audioBuffer;
    bool m_isRecording;
    bool m_serverReady;
    
    // VAD parameters
    double m_silenceThreshold;
    int m_minChunkDurationMs;
    int m_maxChunkDurationMs;
    int m_silenceDurationMs;
    
    QTimer* m_processTimer;
    int m_currentSilenceDuration;
    
    // For auto-send after silence
    QTimer* m_silenceTimer;
    bool m_hasRecognizedSpeech;
};
