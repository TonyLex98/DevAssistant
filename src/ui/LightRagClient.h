#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

class LightRagClient : public QObject
{
    Q_OBJECT

public:
    explicit LightRagClient(QObject *parent = nullptr);
    ~LightRagClient();

    void setSettings(const QString &host, const QString &embeddingModel, 
                     const QString &llmModel, const QString &indexPath);

    void indexFile(const QString &filePath);
    void chatQuery(const QString &query, const QString &model = QString());
    void getGraphData();

signals:
    void chatResponse(const QString &response);
    void chatStreamResponse(const QString &response);  // For streaming responses
    void chatError(const QString &error);
    void graphDataReceived(const QJsonObject &graphData);
    void indexProgress(const QString &filePath, int progress);
    void indexComplete(const QString &filePath, bool success, const QString &message);

private slots:
    void onNetworkReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_host;
    QString m_embeddingModel;
    QString m_llmModel;
    QString m_indexPath;
    QHash<QNetworkReply*, QString> m_pendingRequests; // Track which file each request is for
    QString m_currentQuery; // Keep track of current query for streaming responses

    void sendRequest(const QString &endpoint, const QJsonObject &payload, const QString &filePath = QString());
};