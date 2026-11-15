#include "ui/controllers/TaskStatusController.h"
#include "core/model/LlmClient.h"
#include <QStatusBar>
#include <QTime>

TaskStatusController::TaskStatusController(QObject* parent)
    : QObject(parent)
    , m_statusWidget(nullptr)
    , m_timer(nullptr)
    , m_totalElapsedTimeMs(0)
    , m_taskRunning(false)
    , m_timerActive(false)
    , m_model(nullptr)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TaskStatusController::onUpdateTick);
    m_timer->start(1000); // Update every second
}

TaskStatusController::~TaskStatusController()
{
}

void TaskStatusController::initStatusBar(QStatusBar* bar)
{
    m_statusWidget = new QLabel();
    m_statusWidget->setStyleSheet("QLabel { background-color: #f0f0f0; border: 1px solid gray; padding: 2px; }");
    m_statusWidget->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusWidget->setMinimumWidth(300);
    m_statusWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    bar->addPermanentWidget(m_statusWidget);
    onUpdateTick();
}

void TaskStatusController::setModel(LlmClient* model)
{
    m_model = model;
}

void TaskStatusController::setCurrentPrompt(const QString& prompt)
{
    m_currentPrompt = prompt;
    onUpdateTick();
}

void TaskStatusController::startTask()
{
    if (!m_taskRunning) {
        m_elapsedTime.start();
        m_taskRunning = true;
        m_timerActive = true;
        m_totalElapsedTimeMs = 0;
    } else {
        m_timerActive = true;
    }
    onUpdateTick();
}

void TaskStatusController::stopTask()
{
    if (m_taskRunning) {
        m_totalElapsedTimeMs += m_elapsedTime.elapsed();
    }
    m_timerActive = false;
    m_taskRunning = false;
    onUpdateTick();
}

void TaskStatusController::onUpdateTick()
{
    if (m_statusWidget && m_model)
        m_statusWidget->setText(buildStatusText());
}

QString TaskStatusController::buildStatusText() const
{
    // Time
    QString timeStr;
    if (m_timerActive) {
        int totalMs = m_totalElapsedTimeMs;
        if (m_taskRunning)
            totalMs += m_elapsedTime.elapsed();
        QTime t = QTime(0, 0).addMSecs(totalMs);
        timeStr = t.toString("hh:mm:ss");
    } else if (m_totalElapsedTimeMs > 0) {
        QTime t = QTime(0, 0).addMSecs(m_totalElapsedTimeMs);
        timeStr = t.toString("hh:mm:ss");
    } else {
        timeStr = "00:00:00";
    }

    // Tokens: the last prompt against the model's context window (num_ctx in the settings).
    const quint64 totalTokens = qMax(1, m_model->options().value(QStringLiteral("num_ctx"), 4096).toInt());
    const quint64 usedTokens = m_currentPrompt.isEmpty()
            ? 0 : qMin<quint64>(totalTokens, m_model->estimateTokens(m_currentPrompt));
    const double pers = 100.0 - double(usedTokens) / double(totalTokens) * 100.0;

    return tr("Time: %1 | Context left: %2/%3 (%4%)")
        .arg(timeStr)
        .arg(formatNumber(usedTokens))
        .arg(formatNumber(totalTokens))
        .arg(QString::number(pers, 'f', 2));
}

QString TaskStatusController::formatNumber(quint64 num) const
{
    if (num >= 1000000) {
        double value = static_cast<double>(num) / 1000000.0;
        if (value == static_cast<int>(value))
            return tr("%1M").arg(QString::number(static_cast<int>(value)));
        return tr("%1M").arg(QString::number(value, 'f', 2));
    } else if (num >= 1000) {
        double value = static_cast<double>(num) / 1000.0;
        if (value == static_cast<int>(value))
            return tr("%1k").arg(QString::number(static_cast<int>(value)));
        return tr("%1k").arg(QString::number(value, 'f', 2));
    }
    return QString::number(num);
}
