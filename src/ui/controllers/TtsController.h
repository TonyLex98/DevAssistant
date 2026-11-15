#pragma once

#include <QObject>
#include <QWidget>
#include <QSettings>

class TtsRunner;
class StyledButton;

// Reading answers aloud: the on/off button (saved in the settings) and the TTS server.
class TtsController : public QObject
{
    Q_OBJECT

public:
    explicit TtsController(QObject* parent = nullptr);
    ~TtsController() override;

    // Returns the toggle button to add to a layout
    StyledButton* toggleButton() const { return m_toggleBtn; }


    // Reads the text aloud if speech is on and the server is ready; otherwise does nothing.
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
};
