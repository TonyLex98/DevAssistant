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

// Структура параметров генерации для Ollama
struct OllamaGenerationParams {
    double temperature = 0.1;        // Очень низкое значение для детерминированного JSON-вывода
    double top_p = 0.9;              // Ограничение вероятностного сэмплирования
    int top_k = 40;                  // Количество токенов-кандидатов
    double repeat_penalty = 1.1;     // Штраф за повторения
    double presence_penalty = 0.0;   // Поощрение новых токенов
    double frequency_penalty = 0.0;  // Штраф за частые токены
    int num_predict = -1;            // Число предсказываемых токенов (-1 = без ограничения)
    int num_ctx = 64000;              // Увеличенный размер контекста по умолчанию
    int mirostat = 0;                // Алгоритм стабилизации (выключен)
    double mirostat_tau = 5.0;       // Чувствительность mirostat
    double mirostat_eta = 0.1;       // Шаг адаптации mirostat
    quint64 seed = 0;                // Фиксация случайности (0 = случайный)
    QStringList stop;                // Список токенов для остановки
    QString reasoning_effort = "medium"; // Уровень рассуждений (для gpt-oss)
    double temperature_decay = 0.0;  // Скорость затухания температуры
    int repeat_last_n = 64;          // Длина истории для повторного штрафа

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
        if (seed != 0) obj["seed"] = qint64(seed);  // Используем qint64 для избежания проблем с типом
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
    explicit OllamaAdapter(QObject* parent=nullptr);

    void setEndpoint(const QUrl& url);
    void configure(const QString& model, const QVariantMap& options) override;
    QString generate(const QString& prompt, const QVariantMap& options = QVariantMap()) override;
    QString generateWithSystem(const QString& userPrompt,
                               const QString& systemPrompt,
                               const QVariantMap& options = QVariantMap());
    int estimateTokens(const QString& text) const override;
    bool isAvailable(QString* errorOut = nullptr) override;
    QStringList listModels();

    QString selectedModel() const { return m_model; }
    QJsonObject lastRequestBody() const { return m_lastBody; }
    
    // Методы для управления параметрами генерации
    void setGenerationParams(const OllamaGenerationParams& params) { m_genParams = params; }
    const OllamaGenerationParams& generationParams() const { return m_genParams; }
    
private:
    OllamaGenerationParams m_genParams;  // Параметры генерации

signals:
    void streamChunk(const QString&);
    void streamFinished();
    void streamError(const QString&);

public slots:
    void startStream(const QString& prompt, const QVariantMap& options = QVariantMap());
    void startStreamWithSystem(const QString& userPrompt,
                               const QString& systemPrompt,
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
    QUrl m_endpoint;
    QString m_model;
    QVariantMap m_params;
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_streamReply;
    QByteArray m_buffer;
    QJsonObject m_lastBody;
};
