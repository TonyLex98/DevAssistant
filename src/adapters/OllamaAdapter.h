#pragma once
#include <QObject>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QVariantMap>
#include <QStringList>
#include "../core/ModelAdapter.h"

// Ollama generation parameters
struct OllamaGenerationParams {
    double temperature = 1;          // sampling temperature
    double top_p = 0.9;              // nucleus sampling cutoff
    int top_k = 40;                  // number of candidate tokens
    double repeat_penalty = 1.1;     // repetition penalty
    double presence_penalty = 0.0;   // rewards new tokens
    double frequency_penalty = 0.0;  // penalizes frequent tokens
    int num_predict = -1;            // tokens to predict (-1 = unlimited)
    int num_ctx = 64000;             // context size (raised default)
    int mirostat = 0;                // stabilization algorithm (off)
    double mirostat_tau = 5.0;       // mirostat sensitivity
    double mirostat_eta = 0.1;       // mirostat learning rate
    quint64 seed = 0;                // fixed seed (0 = random)
    QStringList stop;                // stop tokens
    QString reasoning_effort = "medium"; // reasoning level (for gpt-oss)
    double temperature_decay = 0.0;  // temperature decay rate
    int repeat_last_n = 64;          // history length for the repeat penalty

    QJsonObject toJsonObject() const {
        QJsonObject obj;
        obj["temperature"] = temperature;
        obj["top_p"] = top_p;
        obj["top_k"] = top_k;
        obj["repeat_penalty"] = repeat_penalty;
        obj["presence_penalty"] = presence_penalty;
        obj["frequency_penalty"] = frequency_penalty;
        obj["num_predict"] = num_predict;
        obj["num_ctx"] = num_ctx;
        // Skip mirostat parameters as they might not be supported by all models
        if (seed != 0) obj["seed"] = qint64(seed);  // qint64 avoids JSON type issues
        if (!stop.isEmpty()) {
            QJsonArray stopArray;
            for (const QString& s : stop)
                stopArray.append(s);
            obj["stop"] = stopArray;
        }
        // Skip reasoning_effort as it's not a standard Ollama parameter
        // Skip temperature_decay as it's not a standard Ollama parameter
        obj["repeat_last_n"] = repeat_last_n;
        return obj;
    }
};

class OllamaAdapter : public QObject, public ModelAdapter {
    Q_OBJECT
public:
    enum class Provider {
        Ollama,
        LlamaCpp
    };

    explicit OllamaAdapter(QObject* parent=nullptr);

    void setEndpoint(const QUrl& url);
    QUrl endpoint() const { return m_endpoint; }
    void setProvider(Provider provider);
    void setProvider(const QString& providerName);
    Provider provider() const { return m_provider; }
    QString providerName() const;
    static Provider providerFromString(const QString& providerName);
    static QString providerToString(Provider provider);
    static QUrl defaultEndpointForProvider(Provider provider);

    void configure(const QString& model, const QVariantMap& options) override;
    QString generate(const QString& prompt, const QVariantMap& options = QVariantMap()) override;
    QString generateWithSystem(const QString& userPrompt,
                               const QString& systemPrompt,
                               const QVariantMap& options = QVariantMap());
    QString generateWithSystemAndImages(const QString& userPrompt,
                                        const QString& systemPrompt,
                                        const QStringList& imagesBase64,
                                        const QVariantMap& options = QVariantMap());
    int estimateTokens(const QString& text) const override;
    bool isAvailable(QString* errorOut = nullptr) override;
    QStringList listModels();

    QString selectedModel() const override { return m_model; }
    QJsonObject lastRequestBody() const { return m_lastBody; }
    
    // Generation parameters
    void setGenerationParams(const OllamaGenerationParams& params) { m_genParams = params; }
    const OllamaGenerationParams& generationParams() const { return m_genParams; }
    
private:
    OllamaGenerationParams m_genParams;

signals:
    void streamChunk(const QString&);
    void streamFinished();
    void streamError(const QString&);

public slots:
    void startStream(const QString& prompt, const QVariantMap& options = QVariantMap());
    void startStreamWithSystem(const QString& userPrompt,
                               const QString& systemPrompt,
                               const QVariantMap& options = QVariantMap());
    void startStreamWithSystemAndImages(const QString& userPrompt,
                                        const QString& systemPrompt,
                                        const QStringList& imagesBase64,
                                        const QVariantMap& options = QVariantMap());
    void stopStream();

private slots:
    void onReadyRead();
    void onStreamFinished();

private:
    QJsonObject buildRequestBody(const QString& prompt,
                                 const QVariantMap& options,
                                 bool stream,
                                 const QString& systemPrompt) const;
    QJsonObject buildLlamaCppRequestBody(const QString& userPrompt,
                                         const QString& systemPrompt,
                                         const QStringList& imagesBase64,
                                         const QVariantMap& options,
                                         bool stream) const;
    QJsonArray buildMessages(const QString& userPrompt,
                             const QString& systemPrompt,
                             const QStringList& imagesBase64) const;
    QUrl m_endpoint;
    Provider m_provider = Provider::Ollama;
    QString m_model;
    QVariantMap m_params;
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_streamReply;
    QByteArray m_buffer;
    QJsonObject m_lastBody;
};
