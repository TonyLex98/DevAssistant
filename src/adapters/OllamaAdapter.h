#pragma once
#include <QObject>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QUrl>
#include "../core/ModelAdapter.h"

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
