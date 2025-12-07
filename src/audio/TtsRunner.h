#pragma once

#include <QObject>
#include <QProcess>

class TtsRunner : public QObject {
    Q_OBJECT
public:
    explicit TtsRunner(QObject* parent = nullptr);
    ~TtsRunner();

    void speak(const QString& text);
    void stop();
    bool isReady() const { return m_ready; }

signals:
    void speakingFinished();
    void statusMessage(const QString& msg);
    void errorOccurred(const QString& error);

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void checkAndInstallDependencies();
    void startServer();

    QProcess* m_process = nullptr;
    bool m_ready = false;
};
