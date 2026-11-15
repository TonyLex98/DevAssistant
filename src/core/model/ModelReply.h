#pragma once
#include <QObject>
#include <QString>
#include <functional>

// One request to a model, in flight or done. A client creates it and reports the answer
// through it; the caller connects to chunk() and finished() and deletes the reply (deleteLater)
// once it has finished. Nothing here blocks the calling thread.
class ModelReply : public QObject
{
    Q_OBJECT
public:
    explicit ModelReply(QObject* parent = nullptr);

    bool isFinished() const { return m_finished; }
    bool isCancelled() const { return m_cancelled; }
    bool hasError() const { return !m_error.isEmpty(); }
    // The answer: the whole text once finished, the part received so far while streaming.
    QString text() const { return m_text; }
    // Why the request failed; empty on success.
    QString errorString() const { return m_error; }

    // Stops the request. The reply finishes at once with isCancelled() set.
    void abort();

    // Waits until the reply has finished. For worker threads only: on the GUI thread a wait
    // would freeze the window, so there it returns false at once.
    bool waitForFinished();

    // For clients: how to stop the transport, the text as it arrives and the outcome.
    // finished() is emitted exactly once, by the first of finishWithText/finishWithError/abort.
    void setAbortHandler(std::function<void()> handler) { m_abortHandler = std::move(handler); }
    void appendText(const QString& part);
    void finishWithText(const QString& text);
    void finishWithError(const QString& error);

signals:
    // A piece of a streamed answer.
    void chunk(const QString& part);
    void finished();

private:
    bool finish();

    QString m_text;
    QString m_error;
    bool m_finished = false;
    bool m_cancelled = false;
    std::function<void()> m_abortHandler;
};
