#include "Encoder.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QDebug>

namespace Rag
{

Encoder::Encoder()
{
    // Nothing special; QNAM works on default thread pool
}

QVector<float> Encoder::embed(const QString &text)
{
    // Cache simple string-to-vector map
    QString key = QString::fromUtf8(text.toUtf8());
    if (mCache.contains(key)) {
        return mCache[key];
    }
    auto vec = requestEmbedding(text);
    mCache[key] = vec;
    return vec;
}

QString Encoder::generateAnswer(const QString &question, const QString &context)
{
    // Build a simple prompt for the LLM
    QString prompt = "Answer the question based on the context below:\n";
    prompt += context;
    prompt += "\nQuestion: " + question;
    return requestLLM(prompt);
}

QVector<float> Encoder::requestEmbedding(const QString &text)
{
    QJsonObject payload;
    payload["model"] = "gemma:7b"; // or appropriate model string
    payload["input"] = text;
    QString json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkRequest req(QUrl("http://localhost:11434/api/embeddings"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QByteArray data = json.toUtf8();
    QNetworkReply *reply = mManager.post(req, data);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Embedding error:" << reply->errorString();
        reply->deleteLater();
        return {};
    }
    QByteArray resp = reply->readAll();
    reply->deleteLater();
    QJsonDocument j = QJsonDocument::fromJson(resp);
    QJsonArray arr = j.array();
    QVector<float> vec;
    for (const QJsonValue &v : arr) vec.push_back(v.toDouble());
    return vec;
}

QString Encoder::requestLLM(const QString &prompt)
{
    QJsonObject payload;
    payload["model"] = "gemma:7b";
    payload["prompt"] = prompt;
    QString json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkRequest req(QUrl("http://localhost:11434/api/chat"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QByteArray data = json.toUtf8();
    QNetworkReply *reply = mManager.post(req, data);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "LLM error:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }
    QByteArray resp = reply->readAll();
    reply->deleteLater();
    QJsonDocument j = QJsonDocument::fromJson(resp);
    // Ollama API returns choices array, pick first message content
    QJsonArray arr = j["choices"].toArray();
    if (arr.isEmpty()) return QString();
    QJsonObject msg = arr[0]["message"].toObject();
    return msg["content"].toString();
}
} // namespace Rag
