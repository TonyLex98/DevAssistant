#pragma once

#include <QObject>
#include <QWidget>
#include <QSettings>

class TtsRunner;
class StyledButton;
class ChatDialog;

/*
 * Encapsulates TTS (text-to-speech) toggle and runner management.
 * Handles persistence of enabled/disabled state in QSettings.
 */
class TtsController : public QObject
{
    Q_OBJECT

public:
    explicit TtsController(QObject* parent = nullptr);
    ~TtsController() override;

    // Returns the toggle button to add to a layout
    StyledButton* toggleButton() const { return m_toggleBtn; }

    // Set chat dialog reference for resumeRecordingAfterResponse
    void setChatDialog(ChatDialog* chat);

    // Speak text if TTS is enabled and runner is ready
    void speak(const QString& text);

    bool isEnabled() const { return m_enabled; }
    TtsRunner* runner() const { return m_runner; }

signals:
    void statusMessage(const QString& msg);
    void errorMessage(const QString& err);

private slots:
    void onToggleClicked();

private:
    void initRunner();
    void cleanupRunner();

    bool        m_enabled;
    TtsRunner*  m_runner;
    StyledButton* m_toggleBtn;
    ChatDialog* m_chat; // weak ref for resumeRecordingAfterResponse
};
