#pragma once
#include <QObject>
#include <QProcess>

class StreamRunner : public QObject {
    Q_OBJECT
public:
    explicit StreamRunner(QObject* parent=nullptr);

    void start(const QString& exePath,
               const QString& modelPath,
               const QString& language = QStringLiteral("ru"),
               int stepMs = 2000,
               int lengthMs = 8000,
               int keepMs = 400,
               int threads = 4,
               int deviceId = -1);
    void stop();
    bool isRunning() const { return m_proc.state() != QProcess::NotRunning; }

signals:
    void textReady(const QString& text);
    void finished(int code, QProcess::ExitStatus st);
    void error(const QString& msg);

private:
    QProcess m_proc;
};
