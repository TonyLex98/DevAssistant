#include "TaskStatusController.h"
#include "OllamaAdapter.h"
#include "ContextManager.h"
#include <QStatusBar>
#include <QDebug>

TaskStatusController::TaskStatusController(QObject* parent)
    : QObject(parent)
    , m_statusWidget(nullptr)
    , m_timer(nullptr)
    , m_totalElapsedTimeMs(0)
    , m_taskRunning(false)
    , m_timerActive(false)
    , m_model(nullptr)
    , m_contextManager(nullptr)
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

void TaskStatusController::setModel(OllamaAdapter* model)
{
    m_model = model;
}

void TaskStatusController::setContextManager(ContextManager* mgr)
{
    m_contextManager = mgr;
}

void TaskStatusController::setCurrentPrompt(const QString& prompt)
{
    m_currentPrompt = prompt;
}

void TaskStatusController::setContextContent(const QString& content)
{
    m_contextContent = content;
}

void TaskStatusController::setStreamBuf(const QString& buf)
{
    m_streamBuf = buf;
}

void TaskStatusController::startTask()
{
    qDebug() << "[TaskStatusController] startTask. m_taskRunning:" << m_taskRunning << "m_timerActive:" << m_timerActive;
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
    qDebug() << "[TaskStatusController] stopTask. m_timerActive:" << m_timerActive << "m_taskRunning:" << m_taskRunning;
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

    // Tokens
    quint64 usedTokens = 0;
    quint64 totalTokens = m_model->generationParams().num_ctx;

    if (!m_currentPrompt.isEmpty()) {
        usedTokens = m_model->estimateTokens(m_currentPrompt);
    } else if (m_contextManager) {
        usedTokens = m_model->estimateTokens(m_contextManager->generateContextText());
    } else {
        usedTokens = m_model->estimateTokens(m_contextContent);
    }

    if (!m_streamBuf.isEmpty())
        usedTokens += m_model->estimateTokens(m_streamBuf);

    if (usedTokens > totalTokens)
        usedTokens = totalTokens;

    double pers = 100.0 - (double)usedTokens / totalTokens * 100.0;

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
