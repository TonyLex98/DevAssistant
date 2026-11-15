#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QAudioFormat>
#include <QAudioInput>   // Qt5; for Qt6: #include <QAudioSource>
#include "audio/WavWriter.h"

class MicTranscriber : public QObject {
    Q_OBJECT
public:
    explicit MicTranscriber(QObject* parent = nullptr);

    void setWhisperExe(const QString& path) { m_whisperExe = path; }
    void setModelPath(const QString& path)  { m_modelPath  = path; }
    void setLanguage(const QString& lang)   { m_language   = lang; } // e.g. "ru", or "" for auto
    void setChunkMs(int ms)                 { m_chunkMs    = ms; }

public slots:
    bool start();
    void stop();

signals:
    void textReady(const QString& text);
    void error(const QString& msg);

private slots:
    void onReadyRead();
    void onChunkTimeout();
    void onProcFinished(int exitCode, QProcess::ExitStatus st);

private:
    void startNewChunk();
    void closeChunkAndRunWhisper();

    QString m_whisperExe;
    QString m_modelPath;
    QString m_language = "ru";
    int m_chunkMs = 5000;

    // Audio
    QAudioFormat m_fmt;
    QScopedPointer<QAudioInput> m_in;   // Qt5; for Qt6: QScopedPointer<QAudioSource>
    QIODevice* m_dev = nullptr;

    // Current chunk
    WavWriter m_writer;
    QString m_curWav;
    QString m_curTxt;

    // whisper process
    QProcess* m_proc = nullptr;

    QTimer m_timer;
};
