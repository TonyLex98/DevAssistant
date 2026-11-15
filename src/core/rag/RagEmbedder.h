#pragma once
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

namespace Rag {

// Gets embedding vectors from the model server.
//
// Ollama: POST /api/embed {"model", "input": [...]}. OpenAI-compatible servers (llama.cpp,
// LM Studio, vLLM...): POST /v1/embeddings. Calls block until the answer arrives (or the
// timeout passes), so use the embedder from a worker thread, not from the UI.
class Embedder
{
public:
    enum class Api { Ollama, OpenAiCompatible };

    Embedder(const QUrl& endpoint, Api api, const QString& model, const QString& apiKey = QString());

    // One vector per text, in order. Empty on failure, with the reason in `error`.
    QVector<QVector<float>> embed(const QStringList& texts, QString* error) const;

    QString model() const { return m_model; }
    static Api apiForProvider(const QString& providerName);

private:
    QUrl m_endpoint;
    Api m_api;
    QString m_model;
    QString m_apiKey;
};

} // namespace Rag
