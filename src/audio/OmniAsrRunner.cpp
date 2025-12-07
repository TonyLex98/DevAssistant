#include "OmniAsrRunner.h"
#include <QTemporaryFile>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QAudioDeviceInfo>

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

    checkAndInstallDependencies();
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
    QString scriptPath = QCoreApplication::applicationDirPath() + "/scripts/run_omni_asr.py";
    
    // Check if we are in debug/release subdir
    if (!QFile::exists(scriptPath)) {
         scriptPath = QCoreApplication::applicationDirPath() + "/../scripts/run_omni_asr.py";
    }
    // Check source dir (hardcoded fallback)
    if (!QFile::exists(scriptPath)) {
        scriptPath = "c:/Users/user/Documents/DevAssistantQt_qt5_qmake/DevAss/scripts/run_omni_asr.py";
    }

    QStringList args;
    args << scriptPath << m_tempFilePath;

    // Assuming 'python' is in PATH. If not, user might need to configure it.
    // We could use a setting for python path.
    qDebug() << "Starting ASR process: python" << args;
    m_process->start("python", args);
}

void OmniAsrRunner::onAudioReady() {
    if (!m_audioDevice) return;
    QByteArray data = m_audioDevice->readAll();
    m_wavWriter.write(data.constData(), data.size());
}

void OmniAsrRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QString stderrOutput = QString::fromLocal8Bit(m_process->readAllStandardError());
    if (!stderrOutput.isEmpty()) {
        qDebug() << "ASR Process Stderr:" << stderrOutput;
    }

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        emit errorOccurred("ASR Process failed: " + stderrOutput);
    } else {
        QByteArray outputData = m_process->readAllStandardOutput();
        QString output = QString::fromUtf8(outputData).trimmed();
        qDebug() << "ASR Process Output:" << output;
        emit recognitionFinished(output);
    }
    // Optional: remove temp file
    // QFile::remove(m_tempFilePath);
}

void OmniAsrRunner::checkAndInstallDependencies() {
    // Check if torchaudio is installed
    QProcess checkProcess;
    checkProcess.start("python", QStringList() << "-c" << "import torchaudio; print('ok')");
    checkProcess.waitForFinished();
    
    if (checkProcess.exitCode() != 0) {
        qDebug() << "Dependencies missing. Installing...";
        // Install dependencies
        QProcess installProcess;
        // Use pip to install requirements
        // We assume requirements.txt is in scripts folder
        QString scriptPath = QCoreApplication::applicationDirPath() + "/scripts/requirements.txt";
        if (!QFile::exists(scriptPath)) {
             scriptPath = QCoreApplication::applicationDirPath() + "/../scripts/requirements.txt";
        }
        if (!QFile::exists(scriptPath)) {
            scriptPath = "c:/Users/user/Documents/DevAssistantQt_qt5_qmake/DevAss/scripts/requirements.txt";
        }

        QStringList args;
        args << "-m" << "pip" << "install" << "torch" << "torchaudio" << "transformers" << "soundfile" << "pyctcdecode" << "faster-whisper";
        // args << "-r" << scriptPath; // Fallback to direct list if file not found logic is complex, but direct list is safer for now.
        
        installProcess.start("python", args);
        installProcess.waitForFinished(-1); // Wait indefinitely
        
        if (installProcess.exitCode() == 0) {
            qDebug() << "Dependencies installed successfully.";
        } else {
            qDebug() << "Failed to install dependencies: " << installProcess.readAllStandardError();
        }
    }
}
