#include "StreamRunner.h"
#include <QTextCodec>

StreamRunner::StreamRunner(QObject* parent) : QObject(parent) {
    m_proc.setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(&m_proc, &QProcess::readyReadStandardOutput, this, [this]{
        const QByteArray raw = m_proc.readAllStandardOutput();
        // Decode as UTF-8; whisper-stream usually prints UTF-8
        const QString out = QString::fromUtf8(raw);
        if (!out.isEmpty()) emit textReady(out);
    });
    QObject::connect(&m_proc,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     this, &StreamRunner::finished);
    QObject::connect(&m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e){
        Q_UNUSED(e);
        emit error(QStringLiteral("Process error: %1").arg(m_proc.errorString()));
    });
}

void StreamRunner::start(const QString& exePath,
                         const QString& modelPath,
                         const QString& language,
                         int stepMs,
                         int lengthMs,
                         int keepMs,
                         int threads,
                         int deviceId) {
    if (isRunning()) return;
    QStringList args;
    args << "-m" << modelPath;
    if (!language.isEmpty()) args << "-l" << language;
    args << "--step"   << QString::number(stepMs);
    args << "--length" << QString::number(lengthMs);
    args << "--keep"   << QString::number(keepMs);
    args << "-t"       << QString::number(threads);
    if (deviceId >= 0) args << "-c" << QString::number(deviceId);
    m_proc.start(exePath, args);
}

void StreamRunner::stop() {
    if (!isRunning()) return;
    m_proc.kill();
    m_proc.waitForFinished(1000);
}
