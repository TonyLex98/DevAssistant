#include "TtsRunner.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>

TtsRunner::TtsRunner(QObject* parent)
    : QObject(parent)
{
    checkAndInstallDependencies();
}

TtsRunner::~TtsRunner()
{
    stop();
}

void TtsRunner::checkAndInstallDependencies()
{
    emit statusMessage("Проверка зависимостей TTS...");
    
    // Check if pyttsx3 is installed
    QProcess checkProcess;
    checkProcess.start("python", QStringList() << "-c" << "import pyttsx3");
    checkProcess.waitForFinished(5000);
    
    if (checkProcess.exitCode() != 0) {
        emit statusMessage("pyttsx3 не найден. Установка...");
        
        QProcess* installProcess = new QProcess(this);
        
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
                emit statusMessage("pyttsx3 успешно установлен.");
                startServer();
            } else {
                emit errorOccurred("Не удалось установить pyttsx3.");
            }
        });
        
        installProcess->start("pip", QStringList() << "install" << "pyttsx3" << "sounddevice");
    } else {
        emit statusMessage("Зависимости TTS найдены.");
        startServer();
    }
}

void TtsRunner::startServer()
{
    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }
    
    m_process = new QProcess(this);
    
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &TtsRunner::onProcessFinished);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TtsRunner::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, [this](){
        QString msg = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
        if (!msg.isEmpty()) {
            qDebug() << "TTS Server Stderr:" << msg;
            emit statusMessage(msg);
        }
    });
    
    // Try multiple paths for the script
    QStringList possiblePaths;
    possiblePaths << QCoreApplication::applicationDirPath() + "/scripts/run_tts_server.py";
    possiblePaths << QCoreApplication::applicationDirPath() + "/../scripts/run_tts_server.py";
    possiblePaths << QCoreApplication::applicationDirPath() + "/../../scripts/run_tts_server.py";
    possiblePaths << "c:/Users/user/Documents/DevAssistantQt_qt5_qmake/DevAss/scripts/run_tts_server.py";
    
    QString scriptPath;
    for (const QString& path : possiblePaths) {
        QFileInfo fi(path);
        if (fi.exists()) {
            scriptPath = fi.absoluteFilePath();
            break;
        }
    }
    
    if (scriptPath.isEmpty()) {
        emit errorOccurred("TTS скрипт не найден!");
        return;
    }
    
    qDebug() << "Starting TTS Server:" << scriptPath;
    
    m_process->start("python", QStringList() << scriptPath);
}

void TtsRunner::onProcessReadyRead()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromLocal8Bit(data).trimmed();
    
    for (const QString& line : output.split('\n')) {
        if (line.trimmed() == "TTS_READY") {
            m_ready = true;
            qDebug() << "TTS Server Ready";
            emit statusMessage("TTS готов к работе.");
        } else if (line.trimmed() == "TTS_DONE") {
            emit speakingFinished();
        }
    }
}

void TtsRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    qDebug() << "TTS Server exited with code:" << exitCode;
    m_ready = false;
}

void TtsRunner::speak(const QString& text)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        emit errorOccurred("TTS сервер не запущен.");
        return;
    }
    
    if (!m_ready) {
        emit errorOccurred("TTS сервер еще не готов.");
        return;
    }
    
    // Send text to the TTS server
    QString toSend = text.trimmed() + "\n";
    m_process->write(toSend.toUtf8());
}

void TtsRunner::stop()
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write("EXIT\n");
        m_process->waitForFinished(2000);
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
        }
    }
}
