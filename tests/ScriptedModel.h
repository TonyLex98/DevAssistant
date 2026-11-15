#pragma once
// A model for tests: answers with prepared texts, in order, and records what it was asked.

#include <QQueue>
#include <QTimer>
#include "core/model/ModelClient.h"

class ScriptedModel : public ModelClient
{
public:
    QString model() const override { return QStringLiteral("scripted"); }

    // The next answer; an empty queue answers with an error.
    void addAnswer(const QString& text) { m_answers.enqueue(text); }
    const QList<ModelRequest>& requests() const { return m_requests; }

    ModelReply* send(const ModelRequest& request) override
    {
        m_requests << request;
        auto* reply = new ModelReply(this);
        const bool hasAnswer = !m_answers.isEmpty();
        const QString text = hasAnswer ? m_answers.dequeue() : QString();
        // Like a real server: the answer comes later, after the caller has connected.
        QTimer::singleShot(0, reply, [reply, text, hasAnswer, stream = request.stream]() {
            if (!hasAnswer) {
                reply->finishWithError(QStringLiteral("no scripted answer left"));
                return;
            }
            if (stream) {
                const int half = text.size() / 2;
                reply->appendText(text.left(half));
                reply->appendText(text.mid(half));
                reply->finishWithText(QString());
            } else {
                reply->finishWithText(text);
            }
        });
        return reply;
    }

private:
    QQueue<QString> m_answers;
    QList<ModelRequest> m_requests;
};
