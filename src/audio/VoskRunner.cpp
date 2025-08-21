
#include "VoskRunner.h"
#include <QAudioDeviceInfo>
#include <QJsonDocument>
#include <QJsonObject>

VoskRunner::VoskRunner(QObject* parent) : QObject(parent) {
    m_fmt.setSampleRate(m_sampleRate);
    m_fmt.setChannelCount(1);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_fmt.setSampleSize(16);
    m_fmt.setCodec("audio/pcm");
    m_fmt.setByteOrder(QAudioFormat::LittleEndian);
    m_fmt.setSampleType(QAudioFormat::SignedInt);
#else
    m_fmt.setSampleFormat(QAudioFormat::Int16);
#endif
}

VoskRunner::~VoskRunner() {
    stop();
    freeVosk();
}

bool VoskRunner::initVosk() {
    if (!m_vosk.isLoaded()) {
        if (m_dllPath.isEmpty())
            return false;
        if (!m_vosk.load(m_dllPath)) {
            emit error(QStringLiteral("Failed to load Vosk DLL: %1").arg(m_vosk.errorString()));
            return false;
        }
    }
    if (m_model == nullptr) {
        if (m_modelPath.isEmpty()) {
            emit error("Model path is empty");
            return false;
        }
        if (m_vosk.set_log_level) m_vosk.set_log_level(0); // silence logs
        m_model = m_vosk.model_new(m_modelPath.toUtf8().constData());
        if (!m_model) {
            emit error("Failed to load Vosk model (check path)");
            return false;
        }
    }
    if (m_rec == nullptr) {
        m_rec = m_vosk.recognizer_new(m_model, float(m_sampleRate));
        if (!m_rec) {
            emit error("Failed to create recognizer");
            return false;
        }
        if (m_vosk.recognizer_set_words) m_vosk.recognizer_set_words(m_rec, m_words ? 1 : 0);
        if (m_vosk.recognizer_set_partial_words) m_vosk.recognizer_set_partial_words(m_rec, m_partialWords ? 1 : 0);
    }
    return true;
}

void VoskRunner::freeVosk() {
    if (m_rec && m_vosk.recognizer_free) { m_vosk.recognizer_free(m_rec); m_rec = nullptr; }
    if (m_model && m_vosk.model_free) { m_vosk.model_free(m_model); m_model = nullptr; }
}

void VoskRunner::resetRecognizer() {
    if (m_rec && m_vosk.recognizer_free) {
        m_vosk.recognizer_free(m_rec);
        m_rec = m_vosk.recognizer_new(m_model, float(m_sampleRate));
        if (m_vosk.recognizer_set_words) m_vosk.recognizer_set_words(m_rec, m_words ? 1 : 0);
        if (m_vosk.recognizer_set_partial_words) m_vosk.recognizer_set_partial_words(m_rec, m_partialWords ? 1 : 0);
    }
}

bool VoskRunner::start() {
    if (!initVosk()) return false;

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (!info.isFormatSupported(m_fmt)) {
        emit error("Requested 16k/mono/16-bit PCM not supported by device");
        return false;
    }
    m_in.reset(new QAudioInput(info, m_fmt, this));
#else
    // Qt6 variant: QAudioSource with QMediaDevices
#endif

    m_dev = m_in->start();
    if (!m_dev) { emit error("Failed to start audio input"); return false; }
    connect(m_dev, &QIODevice::readyRead, this, &VoskRunner::onReadyRead);
    return true;
}

void VoskRunner::stop() {
    if (m_dev) disconnect(m_dev, nullptr, this, nullptr);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    if (m_in) m_in->stop();
#else
    // if (m_in) m_in->stop();
#endif
}

void VoskRunner::onReadyRead() {
    if (!m_dev || !m_rec) return;
    QByteArray data = m_dev->readAll();
    if (data.isEmpty()) return;

    int accepted = m_vosk.recognizer_accept_waveform(m_rec, data.constData(), data.size());
    const char* jstr = nullptr;
    if (accepted) {
        jstr = m_vosk.recognizer_result(m_rec);
        if (jstr) {
            QJsonParseError e; auto doc = QJsonDocument::fromJson(QByteArray(jstr), &e);
            if (e.error == QJsonParseError::NoError && doc.isObject()) {
                QString text = doc.object().value(QStringLiteral("text")).toString();
                if (!text.isEmpty()) emit finalText(text);
            }
        }
    } else {
        jstr = m_vosk.recognizer_partial_result(m_rec);
        if (jstr) {
            QJsonParseError e; auto doc = QJsonDocument::fromJson(QByteArray(jstr), &e);
            if (e.error == QJsonParseError::NoError && doc.isObject()) {
                QString text = doc.object().value(QStringLiteral("partial")).toString();
                if (!text.isEmpty()) emit partialText(text);
            }
        }
    }
}
