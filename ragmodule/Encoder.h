#pragma once

/// @file Encoder.h
/// @brief Wraps Ollama embeddings endpoint.

#include "RAGProcessor.h"
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QMap>
namespace Rag
{
class Encoder
{
public:
    Encoder();
    QVector<float> embed(const QString &text);
    QString generateAnswer(const QString &question, const QString &context);
private:
    QNetworkAccessManager mManager;
    QMap<int, QVector<float>> mCache;
    QVector<float> requestEmbedding(const QString &text);
    QString requestLLM(const QString &prompt);
};
} // namespace Rag

