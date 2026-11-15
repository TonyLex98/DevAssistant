#include "TtsRunner.h"
#include "PythonScripts.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSettings>

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
    // Asynchronous: the window stays responsive, and the result arrives after the owner has
    // connected to our signals.
    PythonScripts::checkPython(this, [this](bool ok, const QString& error) {
        if (!ok) {
            emit errorOccurred(tr("Text to speech: %1").arg(error));
            return;
        }
        startServer();
    });
}

void TtsRunner::startServer()
{
    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    m_ready = false;

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TtsRunner::onProcessFinished);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &TtsRunner::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        const QString msg = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
        if (!msg.isEmpty())
            emit statusMessage(msg);
    });

    const QString scriptPath = PythonScripts::findScript(QStringLiteral("run_tts_server.py"));
    if (scriptPath.isEmpty()) {
        emit errorOccurred(tr("Text to speech: scripts/run_tts_server.py was not found next to the program."));
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString voice = QSettings().value("tts/voice", "").toString().trimmed();
    if (!voice.isEmpty())
        env.insert("DEVASS_TTS_VOICE", voice);
    m_process->setProcessEnvironment(env);

    emit statusMessage(QString("TTS: starting backend (%1)").arg(scriptPath));
    m_process->start(PythonScripts::pythonProgram(), QStringList() << "-u" << scriptPath);
}

void TtsRunner::onProcessReadyRead()
{
    while (m_process && m_process->canReadLine()) {
        const QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        if (line.isEmpty())
            continue;

        if (line == "TTS_READY") {
            m_ready = true;
            emit statusMessage("TTS: ready.");
            continue;
        }
        if (line == "TTS_DONE") {
            emit speakingFinished();
            continue;
        }
        emit statusMessage(line);
    }
}

void TtsRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)
    m_ready = false;
    emit statusMessage(QString("TTS: backend exited with code %1").arg(exitCode));
}

void TtsRunner::speak(const QString& text)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        emit errorOccurred("TTS: backend process is not running.");
        return;
    }

    if (!m_ready) {
        emit errorOccurred("TTS: backend is not ready yet.");
        return;
    }

    QString normalized = text;
    normalized.replace('\n', ' ');
    normalized = normalized.trimmed();
    if (normalized.isEmpty())
        return;

    if (normalized.size() > 1800) {
        normalized = normalized.left(1800) + "...";
    }

    m_process->write((normalized + "\n").toUtf8());
}

void TtsRunner::stop()
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write("EXIT\n");
        m_process->waitForFinished(3000);
        if (m_process->state() == QProcess::Running)
            m_process->kill();
    }
}

