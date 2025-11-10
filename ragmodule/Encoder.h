#pragma once

/// @file Encoder.h
/// @brief Wraps Ollama embeddings endpoint.

#include "RAGProcessor.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QMap>

// Forward declaration
class OllamaAdapter;

namespace Rag
{
class Encoder
{
public:
    Encoder();
    void setOllamaAdapter(OllamaAdapter* adapter);
    QVector<float> embed(const QString &text);
    QString generateAnswer(const QString &question, const QString &context);
private:
    QNetworkAccessManager mManager;
    OllamaAdapter* mOllamaAdapter = nullptr;
    QMap<QString, QVector<float>> mCache;
    QVector<float> requestEmbedding(const QString &text);
    QString requestLLM(const QString &prompt);
};
} // namespace Rag

