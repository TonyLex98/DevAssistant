#pragma once

#include <QObject>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>

class OllamaAdapter;
class ContextManager;
class QStatusBar;

/*
 * Manages task timer and status bar display (elapsed time + token usage).
 * Independent of MainWindow — receives dependencies via setters.
 */
class TaskStatusController : public QObject
{
    Q_OBJECT

public:
    explicit TaskStatusController(QObject* parent = nullptr);
    ~TaskStatusController() override;

    // Initialize status bar widget (call once from MainWindow constructor)
    void initStatusBar(QStatusBar* bar);

    // Dependencies
    void setModel(OllamaAdapter* model);
    void setContextManager(ContextManager* mgr);
    void setCurrentPrompt(const QString& prompt);
    void setContextContent(const QString& content);
    void setStreamBuf(const QString& buf);

    // Timer control
    void startTask();
    void stopTask();
    bool isTimerActive() const { return m_timerActive; }

    // Accessor
    QLabel* statusWidget() const { return m_statusWidget; }

private slots:
    void onUpdateTick();

private:
    QString formatNumber(quint64 num) const;
    QString buildStatusText() const;

    QLabel*   m_statusWidget;
    QTimer*   m_timer;
    QElapsedTimer m_elapsedTime;
    int       m_totalElapsedTimeMs;
    bool      m_taskRunning;
    bool      m_timerActive;

    // Dependencies (weak refs)
    OllamaAdapter* m_model;
    ContextManager* m_contextManager;
    QString         m_currentPrompt;
    QString         m_contextContent;
    QString         m_streamBuf;
};
