#include "audio/OmniAsrRunner.h"
#include "audio/PythonScripts.h"
#include <QTemporaryFile>
#include <QTimer>
#include <QCoreApplication>
#include <QDir>
#include <QAudioDeviceInfo>
#include "core/util/Logging.h"

OmniAsrRunner::OmniAsrRunner(QObject* parent) : QObject(parent), m_audioInput(nullptr), m_audioDevice(nullptr), m_process(nullptr) {
    m_format.setSampleRate(16000);
    m_format.setChannelCount(1);
    m_format.setSampleSize(16);
    m_format.setCodec("audio/pcm");
    m_format.setByteOrder(QAudioFormat::LittleEndian);
    m_format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (!info.isFormatSupported(m_format)) {
        m_format = info.nearestFormat(m_format);
    }

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &OmniAsrRunner::onProcessFinished);
    // After the owner has connected to our signals.
    QTimer::singleShot(0, this, &OmniAsrRunner::checkAndInstallDependencies);
}

OmniAsrRunner::~OmniAsrRunner() {
    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
    }
}

void OmniAsrRunner::startRecording() {
    // Create temp file path
    // We use a fixed name or random one. 
    QString tempDir = QDir::tempPath();
    m_tempFilePath = tempDir + "/omni_asr_recording.wav";

    if (!m_wavWriter.open(m_tempFilePath, m_format.sampleRate(), m_format.channelCount(), m_format.sampleSize())) {
        emit errorOccurred("Failed to open WAV writer at " + m_tempFilePath);
        return;
    }

    m_audioInput = new QAudioInput(m_format, this);
    m_audioDevice = m_audioInput->start();
    connect(m_audioDevice, &QIODevice::readyRead, this, &OmniAsrRunner::onAudioReady);

    emit recordingStarted();
}

void OmniAsrRunner::stopRecording() {
    if (m_audioInput) {
        m_audioInput->stop();
        m_audioInput->deleteLater();
        m_audioInput = nullptr;
    }
    m_wavWriter.close();
    emit recordingStopped();

    // Run process
    // Try to find the script
    const QString scriptPath = PythonScripts::findScript(QStringLiteral("run_omni_asr.py"));
    if (scriptPath.isEmpty()) {
        emit errorOccurred(tr("Omni ASR: scripts/run_omni_asr.py was not found next to the program."));
        return;
    }

    QStringList args;
    args << scriptPath << m_tempFilePath;
    qCDebug(logAudio) << "Starting ASR process:" << PythonScripts::pythonProgram() << args;
    m_process->start(PythonScripts::pythonProgram(), args);
}

void OmniAsrRunner::onAudioReady() {
    if (!m_audioDevice) return;
    QByteArray data = m_audioDevice->readAll();
    m_wavWriter.write(data.constData(), data.size());
}

void OmniAsrRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QString stderrOutput = QString::fromLocal8Bit(m_process->readAllStandardError());
    if (!stderrOutput.isEmpty()) {
        qCDebug(logAudio) << "ASR Process Stderr:" << stderrOutput;
    }

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        emit errorOccurred("ASR Process failed: " + stderrOutput);
    } else {
        QByteArray outputData = m_process->readAllStandardOutput();
        QString output = QString::fromUtf8(outputData).trimmed();
        qCDebug(logAudio) << "ASR Process Output:" << output;
        emit recognitionFinished(output);
    }
    // Optional: remove temp file
    // QFile::remove(m_tempFilePath);
}

void OmniAsrRunner::checkAndInstallDependencies() {
    // Only checks. Installing torch and transformers takes gigabytes and minutes, so it is
    // left to the user instead of being started silently from the UI.
    PythonScripts::checkPython(this, [this](bool ok, const QString& error) {
        if (!ok) {
            emit errorOccurred(tr("Omni ASR: %1").arg(error));
            return;
        }
        auto* check = new QProcess(this);
        connect(check, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, check](int code, QProcess::ExitStatus status) {
            check->deleteLater();
            if (status != QProcess::NormalExit || code != 0)
                emit errorOccurred(tr("Omni ASR needs Python packages: pip install torch torchaudio transformers soundfile"));
        });
        check->start(PythonScripts::pythonProgram(),
                     {QStringLiteral("-c"), QStringLiteral("import torch, torchaudio, transformers, soundfile")});
    });
}
