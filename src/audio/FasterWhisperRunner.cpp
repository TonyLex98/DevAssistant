#include "FasterWhisperRunner.h"
#include <QCoreApplication>
#include <QDir>
#include <QAudioDeviceInfo>
#include <QtMath>
#include <QDateTime>
#include <QSettings>
#include "utils/Logging.h"

FasterWhisperRunner::FasterWhisperRunner(QObject* parent) 
    : QObject(parent)
    , m_audioInput(nullptr)
    , m_audioDevice(nullptr)
    , m_process(nullptr)
    , m_isRecording(false)
    , m_serverReady(false)
    , m_silenceThreshold(0.01) // Adjust based on mic sensitivity
    , m_minChunkDurationMs(1000) // Minimum 1 second for faster response
    , m_maxChunkDurationMs(5000) // Maximum 5 seconds
    , m_silenceDurationMs(300) // 300ms silence to trigger cut
    , m_currentSilenceDuration(0)
    , m_hasRecognizedSpeech(false)
{
    QSettings s;
    m_silenceThreshold = s.value("asr/silenceThreshold", m_silenceThreshold).toDouble();
    m_minChunkDurationMs = s.value("asr/minChunkMs", m_minChunkDurationMs).toInt();
    m_maxChunkDurationMs = s.value("asr/maxChunkMs", m_maxChunkDurationMs).toInt();
    m_silenceDurationMs = s.value("asr/cutSilenceMs", m_silenceDurationMs).toInt();

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

    m_processTimer = new QTimer(this);
    connect(m_processTimer, &QTimer::timeout, this, &FasterWhisperRunner::processAudioChunk);
    
    // Timer for silence detection (3 seconds)
    m_silenceTimer = new QTimer(this);
    m_silenceTimer->setSingleShot(true);
    m_silenceTimer->setInterval(3000); // 3 seconds
    connect(m_silenceTimer, &QTimer::timeout, this, &FasterWhisperRunner::onSilenceTimeout);
    
    // Defer dependency check to allow signals to be connected
    QTimer::singleShot(0, this, &FasterWhisperRunner::checkAndInstallDependencies);
}

FasterWhisperRunner::~FasterWhisperRunner() {
    stopRecording();
    stopServer();
}

void FasterWhisperRunner::checkAndInstallDependencies() {
    emit statusMessage("ASR: checking Python runtime...");

    QProcess checkProcess;
    checkProcess.start("python", QStringList() << "--version");
    checkProcess.waitForFinished(5000);

    if (checkProcess.exitCode() != 0) {
        emit errorOccurred("ASR: Python is not available. Install Python and restart.");
        return;
    }

    startServer();
}
void FasterWhisperRunner::startServer() {
    if (m_process && m_process->state() != QProcess::NotRunning) return;

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FasterWhisperRunner::onProcessFinished);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &FasterWhisperRunner::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, [this](){
        QString msg = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
        if (!msg.isEmpty()) {
            qCDebug(logAudio) << "Whisper Server Stderr:" << msg;
            emit statusMessage(msg);
            QCoreApplication::processEvents(); // Force UI update
        }
    });

    QString scriptPath = QCoreApplication::applicationDirPath() + "/scripts/run_faster_whisper_server.py";
    if (!QFile::exists(scriptPath)) {
         scriptPath = QCoreApplication::applicationDirPath() + "/../scripts/run_faster_whisper_server.py";
    }
    if (!QFile::exists(scriptPath)) {
        scriptPath = "c:/Users/user/Documents/DevAssistantQt_qt5_qmake/DevAss/scripts/run_faster_whisper_server.py";
    }

    qCDebug(logAudio) << "Starting Faster Whisper Server: " << scriptPath;
    m_process->start("python", QStringList() << "-u" << scriptPath);
}

void FasterWhisperRunner::stopServer() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->write("EXIT\n");
        m_process->waitForFinished(1000);
        m_process->kill();
    }
}

void FasterWhisperRunner::startRecording() {
    if (m_isRecording) return;

    if (!m_process || m_process->state() != QProcess::Running) {
        startServer();
    }

    m_audioBuffer.clear();
    m_currentSilenceDuration = 0;
    
    m_audioInput = new QAudioInput(m_format, this);
    m_audioDevice = m_audioInput->start();
    connect(m_audioDevice, &QIODevice::readyRead, this, &FasterWhisperRunner::onAudioReady);
    
    // Check audio chunks every 100ms
    m_processTimer->start(100);
    
    m_isRecording = true;
    emit recordingStarted();
}

void FasterWhisperRunner::stopRecording() {
    if (!m_isRecording) return;

    m_processTimer->stop();
    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;
    }
    
    // Process remaining buffer
    if (!m_audioBuffer.isEmpty()) {
        // Force process
        // Save to file and send
        QString tempPath = QDir::tempPath() + "/whisper_chunk_final_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".wav";
        WavWriter writer;
        if (writer.open(tempPath, m_format.sampleRate(), m_format.channelCount(), m_format.sampleSize())) {
            writer.write(m_audioBuffer.constData(), m_audioBuffer.size());
            writer.close();
            if (m_process && m_process->state() == QProcess::Running) {
                m_process->write((tempPath + "\n").toUtf8());
            }
        }
    }
    
    // Reset silence timer and speech flag
    m_silenceTimer->stop();
    m_hasRecognizedSpeech = false;
    
    m_isRecording = false;
    emit recordingStopped();
}

void FasterWhisperRunner::onAudioReady() {
    if (!m_audioDevice) return;
    QByteArray data = m_audioDevice->readAll();
    m_audioBuffer.append(data);

    // Keep at most ~30 seconds of pending raw audio in memory.
    const int maxBufferedBytes = 16000 * 2 * 30;
    if (m_audioBuffer.size() > maxBufferedBytes) {
        m_audioBuffer = m_audioBuffer.right(maxBufferedBytes);
    }
}

void FasterWhisperRunner::onSilenceTimeout() {
    // After 3 seconds of silence, emit signal to send the message
    emit silenceDetected();
    m_hasRecognizedSpeech = false;
}

double FasterWhisperRunner::calculateRMS(const QByteArray& data) {
    if (data.isEmpty()) return 0.0;
    
    const qint16* samples = reinterpret_cast<const qint16*>(data.constData());
    int sampleCount = data.size() / 2;
    
    double sum = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        double sample = samples[i] / 32768.0; // Normalize to -1.0 to 1.0
        sum += sample * sample;
    }
    
    return qSqrt(sum / sampleCount);
}

void FasterWhisperRunner::processAudioChunk() {
    // Analyze the last chunk of audio (e.g., last 100ms)
    // 16000 Hz * 2 bytes * 0.1s = 3200 bytes
    int bytesToCheck = 3200;
    if (m_audioBuffer.size() < bytesToCheck) return;
    
    QByteArray lastChunk = m_audioBuffer.right(bytesToCheck);
    double rms = calculateRMS(lastChunk);
    
    // Calculate duration of current buffer
    // bytes / (rate * channels * bytesPerSample)
    double durationMs = (m_audioBuffer.size() / (16000.0 * 1 * 2)) * 1000;
    
    if (rms < m_silenceThreshold) {
        m_currentSilenceDuration += 100;
        
        // If we have recognized speech and silence is detected, start the silence timer
        if (m_hasRecognizedSpeech && !m_silenceTimer->isActive()) {
            m_silenceTimer->start();
        }
    } else {
        m_currentSilenceDuration = 0;
        
        // Reset silence timer when speech is detected
        m_silenceTimer->stop();
        m_hasRecognizedSpeech = true;
    }
    
    bool shouldCut = false;
    
    // Condition 1: Silence detected AND minimum duration reached
    if (m_currentSilenceDuration >= m_silenceDurationMs && durationMs >= m_minChunkDurationMs) {
        shouldCut = true;
        // qCDebug(logAudio) << "Cutting due to silence. Duration:" << durationMs;
    }
    
    // Condition 2: Maximum duration reached
    if (durationMs >= m_maxChunkDurationMs) {
        shouldCut = true;
        // qCDebug(logAudio) << "Cutting due to max duration.";
    }
    
    if (shouldCut) {
        // Save buffer to file
        QString tempPath = QDir::tempPath() + "/whisper_chunk_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".wav";
        WavWriter writer;
        if (writer.open(tempPath, m_format.sampleRate(), m_format.channelCount(), m_format.sampleSize())) {
            writer.write(m_audioBuffer.constData(), m_audioBuffer.size());
            writer.close();
            
            // Send to python
            if (m_process && m_process->state() == QProcess::Running) {
                m_process->write((tempPath + "\n").toUtf8());
            } else {
                qCDebug(logAudio) << "Server not running, restarting...";
                startServer();
            }
        }
        
        // Clear buffer
        m_audioBuffer.clear();
        m_currentSilenceDuration = 0;
    }
}

void FasterWhisperRunner::onProcessReadyRead() {
    while (m_process->canReadLine()) {
        QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        if (line == "READY") {
            m_serverReady = true;
            emit serverReady();
            qCDebug(logAudio) << "Faster Whisper Server Ready";
        } else if (line.startsWith("STATUS:")) {
            emit statusMessage(line.mid(QString("STATUS:").size()).trimmed());
        } else if (line.startsWith("ERROR:")) {
            emit errorOccurred(line.mid(QString("ERROR:").size()).trimmed());
        } else if (!line.isEmpty()) {
            emit recognitionFinished(line);
        }
    }
}

void FasterWhisperRunner::onProcessFinished(int exitCode, QProcess::ExitStatus) {
    qCDebug(logAudio) << "Faster Whisper Server exited code:" << exitCode;
    m_serverReady = false;
    // Auto restart if recording?
}
