#pragma once
#include <QObject>
#include <QString>

// Voice input for the chat, with a choice of recognition engine (Settings > Audio):
//
//   faster-whisper  local Python server (scripts/run_faster_whisper_server.py), streaming,
//                   detects the pause after a phrase; needs Python and faster-whisper.
//   Vosk            offline, in-process through libvosk.dll and a Vosk model folder;
//                   light and fast, no Python.
//   whisper.cpp     the whisper.cpp command-line program and a ggml model, fed in chunks.
//   Omni ASR        a wav2vec2 / MMS model through Python and transformers, recognizes the
//                   whole recording when it stops.
//
// Every engine reports the same signals, so the chat does not depend on which one is used.
class SpeechRecognizer : public QObject
{
    Q_OBJECT
public:
    enum class Engine { FasterWhisper, Vosk, WhisperCpp, OmniAsr };

    using QObject::QObject;

    virtual void startRecording() = 0;
    virtual void stopRecording() = 0;
    bool isRecording() const { return m_recording; }

    static Engine configuredEngine();             // the "speech/engine" setting
    static QString engineKey(Engine engine);      // "faster-whisper", "vosk", ...
    static QString engineTitle(Engine engine);    // shown in Settings
    static SpeechRecognizer* create(Engine engine, QObject* parent);

signals:
    void recognitionFinished(const QString& text); // recognized text, appended to the input
    void silenceDetected();                         // the speaker paused after a phrase
    void statusMessage(const QString& message);
    void errorOccurred(const QString& message);

protected:
    bool m_recording = false;
};
