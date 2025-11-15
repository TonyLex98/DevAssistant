#include "OllamaClient.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QDebug>

OllamaClient::OllamaClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished, 
            this, &OllamaClient::onNetworkReplyFinished);
}

void OllamaClient::setHost(const QString &host)
{
    m_host = host;
}

void OllamaClient::getModels()
{
    QUrl url(m_host + "/api/tags");
    QNetworkRequest request(url);
    
    m_networkManager->get(request);
}

void OllamaClient::onNetworkReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit error(QString("Network error: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }
    
    QByteArray responseData = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        emit error(QString("JSON parse error: %1").arg(parseError.errorString()));
        reply->deleteLater();
        return;
    }
    
    QStringList models;
    if (doc.isObject() && doc.object().contains("models")) {
        QJsonArray modelArray = doc.object().value("models").toArray();
        for (const QJsonValue &value : modelArray) {
            if (value.isObject()) {
                QJsonObject modelObj = value.toObject();
                QString name = modelObj.value("name").toString();
                if (!name.isEmpty()) {
                    models << name;
                }
            }
        }
    }
    
    emit modelsReceived(models);
    reply->deleteLater();
}