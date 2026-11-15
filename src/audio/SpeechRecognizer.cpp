#include "SpeechRecognizer.h"

#include "FasterWhisperRunner.h"
#include "MicTranscriber.h"
#include "OmniAsrRunner.h"
#include "VoskRunner.h"

#include <QFileInfo>
#include <QSettings>

namespace {

class FasterWhisperRecognizer : public SpeechRecognizer
{
public:
    explicit FasterWhisperRecognizer(QObject* parent)
        : SpeechRecognizer(parent), m_runner(new FasterWhisperRunner(this))
    {
        connect(m_runner, &FasterWhisperRunner::recognitionFinished, this, &SpeechRecognizer::recognitionFinished);
        connect(m_runner, &FasterWhisperRunner::silenceDetected, this, &SpeechRecognizer::silenceDetected);
        connect(m_runner, &FasterWhisperRunner::statusMessage, this, &SpeechRecognizer::statusMessage);
        connect(m_runner, &FasterWhisperRunner::errorOccurred, this, [this](const QString& e) {
            m_recording = false;
            emit errorOccurred(e);
        });
    }
    void startRecording() override { m_runner->startRecording(); m_recording = true; }
    void stopRecording() override { m_runner->stopRecording(); m_recording = false; }

private:
    FasterWhisperRunner* m_runner;
};

class VoskRecognizer : public SpeechRecognizer
{
public:
    explicit VoskRecognizer(QObject* parent)
        : SpeechRecognizer(parent), m_runner(new VoskRunner(this))
    {
        connect(m_runner, &VoskRunner::finalText, this, [this](const QString& text) {
            if (!text.trimmed().isEmpty()) {
                emit recognitionFinished(text.trimmed());
                emit silenceDetected(); // Vosk ends a phrase at a pause
            }
        });
        connect(m_runner, &VoskRunner::partialText, this, [this](const QString& text) {
            if (!text.trimmed().isEmpty())
                emit statusMessage(tr("Hearing: %1").arg(text.trimmed()));
        });
        connect(m_runner, &VoskRunner::error, this, [this](const QString& e) {
            m_recording = false;
            emit errorOccurred(tr("Vosk: %1").arg(e));
        });
    }
    void startRecording() override
    {
        QSettings s;
        const QString dll = s.value(QStringLiteral("speech/voskDll")).toString();
        const QString model = s.value(QStringLiteral("speech/voskModel")).toString();
        if (!QFileInfo::exists(dll) || !QFileInfo(model).isDir()) {
            emit errorOccurred(tr("Vosk: set libvosk.dll and the model folder in Settings > Audio "
                                  "(models: alphacephei.com/vosk/models)."));
            return;
        }
        m_runner->setDllPath(dll);
        m_runner->setModelPath(model);
        m_runner->setSampleRate(16000);
        m_recording = m_runner->start();
    }
    void stopRecording() override { m_runner->stop(); m_recording = false; }

private:
    VoskRunner* m_runner;
};

class WhisperCppRecognizer : public SpeechRecognizer
{
public:
    explicit WhisperCppRecognizer(QObject* parent)
        : SpeechRecognizer(parent), m_runner(new MicTranscriber(this))
    {
        connect(m_runner, &MicTranscriber::textReady, this, [this](const QString& text) {
            if (!text.trimmed().isEmpty())
                emit recognitionFinished(text.trimmed());
        });
        connect(m_runner, &MicTranscriber::error, this, [this](const QString& e) {
            m_recording = false;
            emit errorOccurred(tr("whisper.cpp: %1").arg(e));
        });
    }
    void startRecording() override
    {
        QSettings s;
        const QString exe = s.value(QStringLiteral("speech/whisperExe")).toString();
        const QString model = s.value(QStringLiteral("speech/whisperModel")).toString();
        if (!QFileInfo::exists(exe) || !QFileInfo::exists(model)) {
            emit errorOccurred(tr("whisper.cpp: set the whisper-cli program and a ggml model in Settings > Audio."));
            return;
        }
        m_runner->setWhisperExe(exe);
        m_runner->setModelPath(model);
        m_runner->setLanguage(s.value(QStringLiteral("speech/language")).toString()); // empty = detect
        m_recording = m_runner->start();
    }
    void stopRecording() override { m_runner->stop(); m_recording = false; }

private:
    MicTranscriber* m_runner;
};

class OmniAsrRecognizer : public SpeechRecognizer
{
public:
    explicit OmniAsrRecognizer(QObject* parent)
        : SpeechRecognizer(parent), m_runner(new OmniAsrRunner(this))
    {
        connect(m_runner, &OmniAsrRunner::recognitionFinished, this, [this](const QString& text) {
            if (!text.trimmed().isEmpty())
                emit recognitionFinished(text.trimmed());
        });
        connect(m_runner, &OmniAsrRunner::errorOccurred, this, [this](const QString& e) {
            m_recording = false;
            emit errorOccurred(e);
        });
    }
    void startRecording() override { m_runner->startRecording(); m_recording = true; }
    void stopRecording() override
    {
        m_runner->stopRecording();
        m_recording = false;
        emit statusMessage(tr("Recognizing the recording..."));
    }

private:
    OmniAsrRunner* m_runner;
};

} // namespace

SpeechRecognizer::Engine SpeechRecognizer::configuredEngine()
{
    const QString key = QSettings().value(QStringLiteral("speech/engine"), engineKey(Engine::FasterWhisper)).toString();
    for (Engine e : {Engine::FasterWhisper, Engine::Vosk, Engine::WhisperCpp, Engine::OmniAsr}) {
        if (engineKey(e) == key)
            return e;
    }
    return Engine::FasterWhisper;
}

QString SpeechRecognizer::engineKey(Engine engine)
{
    switch (engine) {
    case Engine::FasterWhisper: return QStringLiteral("faster-whisper");
    case Engine::Vosk:          return QStringLiteral("vosk");
    case Engine::WhisperCpp:    return QStringLiteral("whisper.cpp");
    case Engine::OmniAsr:       return QStringLiteral("omni-asr");
    }
    return QString();
}

QString SpeechRecognizer::engineTitle(Engine engine)
{
    switch (engine) {
    case Engine::FasterWhisper: return tr("faster-whisper (Python, streaming)");
    case Engine::Vosk:          return tr("Vosk (offline, libvosk.dll)");
    case Engine::WhisperCpp:    return tr("whisper.cpp (command-line program)");
    case Engine::OmniAsr:       return tr("Omni ASR (Python, wav2vec2)");
    }
    return QString();
}

SpeechRecognizer* SpeechRecognizer::create(Engine engine, QObject* parent)
{
    switch (engine) {
    case Engine::Vosk:       return new VoskRecognizer(parent);
    case Engine::WhisperCpp: return new WhisperCppRecognizer(parent);
    case Engine::OmniAsr:    return new OmniAsrRecognizer(parent);
    case Engine::FasterWhisper:
    default:                 return new FasterWhisperRecognizer(parent);
    }
}
