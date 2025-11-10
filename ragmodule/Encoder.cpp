#include "Encoder.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include "../src/adapters/OllamaAdapter.h"

namespace Rag
{

Encoder::Encoder()
{
    // Nothing special; QNAM works on default thread pool
}

void Encoder::setOllamaAdapter(OllamaAdapter* adapter)
{
    mOllamaAdapter = adapter;
}

void Encoder::setEmbeddingModel(const QString &model)
{
    mEmbeddingModel = model;
}

QVector<float> Encoder::embed(const QString &text)
{
    // Cache simple string-to-vector map
    QString key = QString::fromUtf8(text.toUtf8());
    if (mCache.contains(key)) {
        return mCache[key];
    }

    // Use the main application's Ollama adapter for embeddings if available
    if (mOllamaAdapter) {
        // Use OllamaAdapter's getEmbedding method with the selected model
        QVector<float> result = mOllamaAdapter->getEmbedding(text, mEmbeddingModel);
        mCache[key] = result;
        return result;
    }

    // Fallback to direct API call if no adapter is set
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
    
    // Use the main application's Ollama adapter for generation if available
    if (mOllamaAdapter) {
        // For now, using the direct request method
        return requestLLM(prompt);
    }
    
    return requestLLM(prompt);
}

QVector<float> Encoder::requestEmbedding(const QString &text)
{
    // This is a fallback implementation that directly calls Ollama
    // In a real implementation, we would use the main OllamaAdapter
    QJsonObject payload;
    payload["model"] = mEmbeddingModel; // Using the selected embedding model
    payload["input"] = text;
    QString json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkRequest req(QUrl("http://localhost:11434/api/embeddings"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // For Qt 5 compatibility, use setRawHeader instead of setTransferTimeout
    req.setRawHeader("Connection", "close");
    QByteArray data = json.toUtf8();
    QNetworkReply *reply = mManager.post(req, data);
    
    // Use a timer-based approach for timeout in Qt5
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(30000); // 30 second timeout
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeoutTimer.start();
    loop.exec();
    
    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        // Timeout occurred
        reply->abort();
        reply->deleteLater();
        qWarning() << "Embedding request timed out";
        return {};
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Embedding error:" << reply->errorString();
        reply->deleteLater();
        return {};
    }
    QByteArray resp = reply->readAll();
    reply->deleteLater();
    QJsonDocument j = QJsonDocument::fromJson(resp);
    QJsonArray arr = j["embeddings"].toArray();  // Ollama v2+ uses embeddings array
    if (arr.isEmpty()) {
        // Try the old format as fallback
        QJsonArray oldFormat = j.array();
        QVector<float> vec;
        for (const QJsonValue &v : oldFormat) {
            vec.push_back(v.toDouble());
        }
        return vec;
    }
    QVector<float> vec;
    QJsonArray firstEmbedding = arr.at(0).toArray();
    for (const QJsonValue &v : firstEmbedding) {
        vec.push_back(v.toDouble());
    }
    return vec;
}

QString Encoder::requestLLM(const QString &prompt)
{
    QJsonObject payload;
    payload["model"] = "gemma:7b";
    payload["prompt"] = prompt;
    payload["stream"] = false; // Non-streaming for simplicity
    QString json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkRequest req(QUrl("http://localhost:11434/api/generate"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // For Qt 5 compatibility, use setRawHeader instead of setTransferTimeout
    req.setRawHeader("Connection", "close");
    QByteArray data = json.toUtf8();
    QNetworkReply *reply = mManager.post(req, data);
    
    // Use a timer-based approach for timeout in Qt5
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(60000); // 60 second timeout
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeoutTimer.start();
    loop.exec();
    
    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        // Timeout occurred
        reply->abort();
        reply->deleteLater();
        qWarning() << "LLM request timed out";
        return QString();
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "LLM error:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }
    QByteArray resp = reply->readAll();
    reply->deleteLater();
    QJsonDocument j = QJsonDocument::fromJson(resp);
    return j["response"].toString();
}
} // namespace Rag