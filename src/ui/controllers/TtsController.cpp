#include "TtsController.h"
#include "TtsRunner.h"
#include "StyledButton.h"
#include "ChatDialog.h"

TtsController::TtsController(QObject* parent)
    : QObject(parent)
    , m_enabled(true)
    , m_runner(nullptr)
    , m_toggleBtn(nullptr)
    , m_chat(nullptr)
{
    // Off until the user turns it on: it starts a Python server that downloads voices.
    // (A new key, so the old on-by-default value does not carry over.)
    QSettings settings;
    m_enabled = settings.value("speech/ttsEnabled", false).toBool();

    m_toggleBtn = new StyledButton(m_enabled ? tr("Озвучка: Вкл") : tr("Озвучка: Выкл"), nullptr);
    connect(m_toggleBtn, &StyledButton::clicked, this, &TtsController::onToggleClicked);

    if (m_enabled)
        initRunner();
}

TtsController::~TtsController()
{
    cleanupRunner();
}

void TtsController::setChatDialog(ChatDialog* chat)
{
    m_chat = chat;
    if (m_runner) {
        disconnect(m_runner, nullptr, this, nullptr);
        connect(m_runner, &TtsRunner::speakingFinished, this, [this]() {
            if (m_chat && m_chat->isRecording())
                m_chat->resumeRecordingAfterResponse();
        });
    }
}

void TtsController::onToggleClicked()
{
    m_enabled = !m_enabled;
    QSettings settings;
    settings.setValue("speech/ttsEnabled", m_enabled);

    m_toggleBtn->setText(m_enabled ? tr("Озвучка: Вкл") : tr("Озвучка: Выкл"));

    if (m_enabled)
        initRunner();
    else
        cleanupRunner();
}

void TtsController::initRunner()
{
    if (m_runner)
        return;
    m_runner = new TtsRunner(this);
    connect(m_runner, &TtsRunner::statusMessage, this, &TtsController::statusMessage);
    connect(m_runner, &TtsRunner::errorOccurred, this, &TtsController::errorMessage);
    connect(m_runner, &TtsRunner::speakingFinished, this, [this]() {
        if (m_chat && m_chat->isRecording())
            m_chat->resumeRecordingAfterResponse();
    });
}

void TtsController::cleanupRunner()
{
    if (m_runner) {
        m_runner->deleteLater();
        m_runner = nullptr;
    }
}

void TtsController::speak(const QString& text)
{
    if (m_enabled && m_runner && m_runner->isReady())
        m_runner->speak(text);
}
