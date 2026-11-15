#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrl>
#include <functional>
#include "core/model/ModelClient.h"

// A model server: Ollama (/api/chat) or any OpenAI-compatible server such as llama.cpp,
// LM Studio or vLLM (/v1/chat/completions). Requests are asynchronous.
class LlmClient : public ModelClient
{
    Q_OBJECT
public:
    enum class Provider { Ollama, OpenAiCompatible };

    explicit LlmClient(QObject* parent = nullptr);

    void setProvider(Provider provider) { m_provider = provider; }
    Provider provider() const { return m_provider; }
    QString providerName() const { return providerToString(m_provider); }
    // Names as stored in the settings: "ollama" and "llama.cpp".
    static Provider providerFromString(const QString& name);
    static QString providerToString(Provider provider);
    static QUrl defaultEndpoint(Provider provider);

    void setEndpoint(const QUrl& url) { m_endpoint = url; }
    QUrl endpoint() const { return m_endpoint; }
    // Sent as "Authorization: Bearer <key>" when not empty (servers that require a key).
    void setApiKey(const QString& key) { m_apiKey = key; }

    // The model and its generation options (temperature, top_p, num_ctx...), used by every request.
    void configure(const QString& model, const QVariantMap& options);
    void setModel(const QString& model) { m_model = model; }
    QString model() const override { return m_model; }
    QVariantMap options() const { return m_options; }
    // Same server, model and options as another client.
    void copySettingsFrom(const LlmClient& other);

    ModelReply* send(const ModelRequest& request) override;

    // Asks the server which models it offers. `done` runs in `context`'s thread unless
    // `context` is destroyed first.
    void fetchModels(QObject* context,
                     std::function<void(const QStringList& models, const QString& error)> done);

    // The body of the last request (for tests and diagnostics).
    QJsonObject lastRequestBody() const { return m_lastBody; }

    // The request body for a request, as it would be sent (public for tests).
    QJsonObject requestBody(const ModelRequest& request) const;

private:
    QNetworkRequest httpRequest(const QString& path) const;
    QJsonArray messages(const ModelRequest& request) const;
    void handleStreamData(class QNetworkReply* net, ModelReply* reply, QByteArray* buffer);
    QString parseAnswer(const QByteArray& data, QString* error) const;

    Provider m_provider = Provider::Ollama;
    QUrl m_endpoint;
    QString m_apiKey;
    QString m_model;
    QVariantMap m_options;
    QNetworkAccessManager m_nam;
    QJsonObject m_lastBody;
};
