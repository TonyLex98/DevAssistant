#include "MicTranscriber.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QDebug>

MicTranscriber::MicTranscriber(QObject* parent) : QObject(parent) {
    // Audio format: 16 kHz, mono, PCM16
    m_fmt.setSampleRate(16000);
    m_fmt.setChannelCount(1);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_fmt.setSampleSize(16);
    m_fmt.setCodec("audio/pcm");
    m_fmt.setByteOrder(QAudioFormat::LittleEndian);
    m_fmt.setSampleType(QAudioFormat::SignedInt);
#else
    m_fmt.setSampleFormat(QAudioFormat::Int16);
#endif
    connect(&m_timer, &QTimer::timeout, this, &MicTranscriber::onChunkTimeout);
}

bool MicTranscriber::start() {
    if (m_whisperExe.isEmpty() || m_modelPath.isEmpty()) {
        emit error(tr("Set the whisper program and the model path first"));
        return false;
    }

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (!info.isFormatSupported(m_fmt)) {
        emit error(tr("The requested audio format is not supported"));
        return false;
    }
    m_in.reset(new QAudioInput(info, m_fmt, this));
#else
    // Qt6:
    // QMediaDevices devs; auto device = devs.defaultAudioInput();
    // m_in.reset(new QAudioSource(device, m_fmt, this));
#endif

    // where the temporary files go
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (baseDir.isEmpty()) baseDir = QDir::currentPath();
    QDir(baseDir).mkpath("qt_mic_whisper");
    QString dir = baseDir + "/qt_mic_whisper";

    // start the first recording
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_dev = m_in->start();
#else
    // m_dev = m_in->start(); // Qt6
#endif
    if (!m_dev) { emit error(tr("Failed to start audio input")); return false; }

    connect(m_dev, &QIODevice::readyRead, this, &MicTranscriber::onReadyRead);
    startNewChunk();

    m_timer.start(m_chunkMs);
    return true;
}

void MicTranscriber::stop() {
    m_timer.stop();
    if (m_dev) disconnect(m_dev, nullptr, this, nullptr);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    if (m_in) m_in->stop();
#else
    // if (m_in) m_in->stop();
#endif
    if (m_writer.path().size()) m_writer.close();
    if (m_proc) { m_proc->kill(); m_proc->deleteLater(); m_proc = nullptr; }
}

void MicTranscriber::onReadyRead() {
    if (!m_dev) return;
    QByteArray data = m_dev->readAll();
    if (!data.isEmpty()) m_writer.write(data.constData(), data.size());
}

void MicTranscriber::startNewChunk() {
    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (baseDir.isEmpty()) baseDir = QDir::currentPath();
    QString dir = baseDir + "/qt_mic_whisper";
    m_curWav = dir + "/chunk_" + ts + ".wav";
    m_curTxt = dir + "/chunk_" + ts + ".txt";

    if (!m_writer.open(m_curWav, 16000, 1, 16)) {
        emit error(tr("Cannot open the WAV file for writing: %1").arg(m_curWav));
    }
}

void MicTranscriber::onChunkTimeout() {
    closeChunkAndRunWhisper();
    startNewChunk();
}

void MicTranscriber::closeChunkAndRunWhisper() {
    if (m_writer.path().isEmpty()) return;
    m_writer.close();

    if (m_proc) {
        // the previous run is still going: skip this chunk (no queue)
        if (m_proc->state() != QProcess::NotRunning) return;
        m_proc->deleteLater();
        m_proc = nullptr;
    }

    m_proc = new QProcess(this);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MicTranscriber::onProcFinished);

    QStringList args;
    args << "-m" << m_modelPath
         << "-f" << m_curWav
         << "-otxt"
         << "-of" << m_curWav;   // writes chunk_xxx.wav.txt
    if (!m_language.isEmpty())
        args << "-l" << m_language;

    m_proc->setProgram(m_whisperExe);
    m_proc->setArguments(args);
    m_proc->start();
}

void MicTranscriber::onProcFinished(int, QProcess::ExitStatus) {
    const QString outTxt = m_curWav + ".txt"; // since -of is the WAV path
    QFile f(outTxt);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString text = QString::fromUtf8(f.readAll()).trimmed();
        if (!text.isEmpty())
            emit textReady(text);
        f.close();
        QFile::remove(outTxt);
    }
    QFile::remove(m_curWav);
}
