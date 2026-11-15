#include "core/model/ModelReply.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>

ModelReply::ModelReply(QObject* parent)
    : QObject(parent)
{
}

void ModelReply::abort()
{
    if (m_finished)
        return;
    m_cancelled = true;
    m_error = QCoreApplication::translate("ModelReply", "The request was cancelled.");
    if (m_abortHandler) {
        auto handler = std::move(m_abortHandler);
        m_abortHandler = nullptr;
        handler();
    }
    finish();
}

bool ModelReply::waitForFinished()
{
    if (m_finished)
        return true;
    QCoreApplication* app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread())
        return false;
    QEventLoop loop;
    connect(this, &ModelReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    return true;
}

void ModelReply::appendText(const QString& part)
{
    if (m_finished || part.isEmpty())
        return;
    m_text += part;
    emit chunk(part);
}

void ModelReply::finishWithText(const QString& text)
{
    if (m_finished)
        return;
    if (!text.isEmpty())
        m_text = text;
    finish();
}

void ModelReply::finishWithError(const QString& error)
{
    if (m_finished)
        return;
    if (!m_cancelled) {  // aborting the transport reports its own error; keep "cancelled"
        m_error = error.isEmpty()
                ? QCoreApplication::translate("ModelReply", "The model server returned an error.")
                : error;
    }
    finish();
}

bool ModelReply::finish()
{
    if (m_finished)
        return false;
    m_finished = true;
    m_abortHandler = nullptr;
    emit finished();
    return true;
}
