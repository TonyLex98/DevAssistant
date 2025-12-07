#include "FasterWhisperRunner.h"
#include <QCoreApplication>
#include <QDir>
#include <QAudioDeviceInfo>
#include <QDebug>
#include <QtMath>
#include <QDateTime>

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
{
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
    
    // Defer dependency check to allow signals to be connected
    QTimer::singleShot(0, this, &FasterWhisperRunner::checkAndInstallDependencies);
}

FasterWhisperRunner::~FasterWhisperRunner() {
    stopRecording();
    stopServer();
}

void FasterWhisperRunner::checkAndInstallDependencies() {
    emit statusMessage("Проверка зависимостей Faster Whisper...");
    
    QProcess* checkProcess = new QProcess(this);
    connect(checkProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, checkProcess](int exitCode){
        checkProcess->deleteLater();
        if (exitCode != 0) {
            emit statusMessage("Faster Whisper отсутствует. Установка... (это может занять минуту)");
            
            QProcess* installProcess = new QProcess(this);
            
            // Capture output to show progress
            connect(installProcess, &QProcess::readyReadStandardOutput, this, [this, installProcess](){
                 QString s = QString::fromLocal8Bit(installProcess->readAllStandardOutput()).trimmed();
                 if(!s.isEmpty()) emit statusMessage(s);
            });
            connect(installProcess, &QProcess::readyReadStandardError, this, [this, installProcess](){
                 QString s = QString::fromLocal8Bit(installProcess->readAllStandardError()).trimmed();
                 if(!s.isEmpty()) emit statusMessage(s);
            });

            connect(installProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, installProcess](int exitCode){
                installProcess->deleteLater();
                if (exitCode == 0) {
                    emit statusMessage("Faster Whisper успешно установлен.");
                    startServer();
                } else {
                    emit errorOccurred("Не удалось установить Faster Whisper. Проверьте логи.");
                    emit statusMessage("Установка не удалась.");
                }
            });
            
            QStringList args;
            args << "-m" << "pip" << "install" << "faster-whisper";
            installProcess->start("python", args);
        } else {
             emit statusMessage("Зависимости Faster Whisper найдены.");
             startServer();
        }
    });
    
    checkProcess->start("python", QStringList() << "-c" << "import faster_whisper; print('ok')");
}

void FasterWhisperRunner::startServer() {
    if (m_process && m_process->state() != QProcess::NotRunning) return;

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FasterWhisperRunner::onProcessFinished);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &FasterWhisperRunner::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, [this](){
        QString msg = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
        if (!msg.isEmpty()) {
            qDebug() << "Whisper Server Stderr:" << msg;
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

    qDebug() << "Starting Faster Whisper Server: " << scriptPath;
    m_process->start("python", QStringList() << scriptPath);
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
    
    m_isRecording = false;
    emit recordingStopped();
}

void FasterWhisperRunner::onAudioReady() {
    if (!m_audioDevice) return;
    QByteArray data = m_audioDevice->readAll();
    m_audioBuffer.append(data);
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
    } else {
        m_currentSilenceDuration = 0;
    }
    
    bool shouldCut = false;
    
    // Condition 1: Silence detected AND minimum duration reached
    if (m_currentSilenceDuration >= m_silenceDurationMs && durationMs >= m_minChunkDurationMs) {
        shouldCut = true;
        // qDebug() << "Cutting due to silence. Duration:" << durationMs;
    }
    
    // Condition 2: Maximum duration reached
    if (durationMs >= m_maxChunkDurationMs) {
        shouldCut = true;
        // qDebug() << "Cutting due to max duration.";
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
                qDebug() << "Server not running!";
                startServer(); // Try to restart
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
            qDebug() << "Faster Whisper Server Ready";
        } else if (!line.isEmpty()) {
            emit recognitionFinished(line);
        }
    }
}

void FasterWhisperRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qDebug() << "Faster Whisper Server exited code:" << exitCode;
    m_serverReady = false;
    // Auto restart if recording?
}
