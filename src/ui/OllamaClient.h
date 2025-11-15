#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class OllamaClient : public QObject
{
    Q_OBJECT

public:
    explicit OllamaClient(QObject *parent = nullptr);

    void setHost(const QString &host);
    void getModels();

signals:
    void modelsReceived(const QStringList &models);
    void error(const QString &error);

private slots:
    void onNetworkReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_host;
};