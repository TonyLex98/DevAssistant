#include "LightRagClient.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

LightRagClient::LightRagClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished, 
            this, &LightRagClient::onNetworkReplyFinished);
}

LightRagClient::~LightRagClient()
{
}

void LightRagClient::setSettings(const QString &host, const QString &embeddingModel, 
                                 const QString &llmModel, const QString &indexPath)
{
    m_host = host;
    m_embeddingModel = embeddingModel;
    m_llmModel = llmModel;
    m_indexPath = indexPath;
}

void LightRagClient::indexFile(const QString &filePath)
{
    QJsonObject payload;
    payload["file_path"] = filePath;
    payload["embedding_model"] = m_embeddingModel;
    payload["llm_model"] = m_llmModel;

    if (!m_indexPath.isEmpty()) {
        payload["index_path"] = m_indexPath;
    }

    // Use the correct LightRAG API endpoint for indexing
    sendRequest("/v1/index", payload, filePath);
}

void LightRagClient::chatQuery(const QString &query, const QString &model)
{
    QJsonObject payload;
    payload["query"] = query;
    payload["model"] = model.isEmpty() ? m_llmModel : model;

    if (!m_indexPath.isEmpty()) {
        payload["index_path"] = m_indexPath;
    }

    // Try to enable streaming if supported by the API
    payload["stream"] = true;

    m_currentQuery = query;
    // Use the correct LightRAG API endpoint for chat
    sendRequest("/v1/chat/completions", payload);
}

void LightRagClient::sendRequest(const QString &endpoint, const QJsonObject &payload, const QString &filePath)
{
    // Use the correct LightRAG server address
    QString baseUrl = "http://localhost:9621"; // Correct LightRAG server address
    QUrl url(baseUrl + endpoint);
    if (!url.isValid()) {
        emit chatError("Invalid URL: " + url.toString());
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(payload).toJson());

    // Store the file path for this request if provided
    if (!filePath.isEmpty()) {
        m_pendingRequests[reply] = filePath;
    }

    // Connect to handle errors specifically for this request
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            [reply, this]() {
                if (m_pendingRequests.contains(reply)) {
                    QString filePath = m_pendingRequests.take(reply);
                    emit indexComplete(filePath, false, "Network error: " + reply->errorString());
                } else {
                    emit chatError("Network error: " + reply->errorString());
                }
            });
}

void LightRagClient::getGraphData()
{
    QJsonObject payload;
    if (!m_indexPath.isEmpty()) {
        payload["index_path"] = m_indexPath;
    }

    // Use the correct LightRAG API endpoint for getting graph data
    sendRequest("/v1/graph", payload);
}

void LightRagClient::onNetworkReplyFinished(QNetworkReply *reply)
{
    // Remove from pending requests
    QString filePath = m_pendingRequests.take(reply);

    // Check for network errors
    if (reply->error() != QNetworkReply::NoError) {
        QString error = QString("Network error: %1").arg(reply->errorString());
        if (!filePath.isEmpty()) {
            emit indexComplete(filePath, false, error);
        } else {
            emit chatError(error);
        }
        reply->deleteLater();
        return;
    }

    // Read the response
    QByteArray responseData = reply->readAll();

    // Check if this is an index request (has file path)
    if (!filePath.isEmpty()) {
        // This is an index response
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

        bool success = false;
        QString message;

        if (parseError.error == QJsonParseError::NoError) {
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                success = obj.value("success").toBool(false);
                message = obj.value("message").toString("Operation completed");
            }
        } else {
            success = false;
            message = QString("JSON parse error: %1").arg(parseError.errorString());
        }

        emit indexComplete(filePath, success, message);
    } else if (reply->url().toString().contains("/v1/graph")) {
        // This is a graph response
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            emit graphDataReceived(doc.object());
        } else {
            emit chatError(QString("Graph data parse error: %1").arg(parseError.errorString()));
        }
    } else {
        // This is a chat response
        // Check if it's a streaming response (multiple JSON objects separated by newlines)
        QString responseText = QString::fromUtf8(responseData);

        // Check if response looks like streaming response (multiple JSON lines)
        if (responseText.contains('\n')) {
            QStringList lines = responseText.split('\n', QString::SkipEmptyParts);
            QString fullResponse;

            for (const QString &line : lines) {
                QString trimmedLine = line.trimmed();
                if (!trimmedLine.isEmpty()) {
                    QJsonParseError parseError;
                    QJsonDocument doc = QJsonDocument::fromJson(trimmedLine.toUtf8(), &parseError);

                    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                        QJsonObject obj = doc.object();
                        QString partialResponse = obj.value("response").toString();
                        if (!partialResponse.isEmpty()) {
                            fullResponse += partialResponse;
                            emit chatStreamResponse(partialResponse);
                        }
                    }
                }
            }

            if (!fullResponse.isEmpty()) {
                emit chatResponse(fullResponse);
            }
        } else {
            // Regular single JSON response
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

            if (parseError.error == QJsonParseError::NoError) {
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();

                    // For OpenAI-compatible API (like v1/chat/completions), the response format is different
                    // Check if it's an OpenAI-style response
                    if (obj.contains("choices")) {
                        QJsonArray choices = obj.value("choices").toArray();
                        if (choices.size() > 0) {
                            QJsonObject choice = choices[0].toObject();
                            QJsonObject message = choice.value("message").toObject();
                            QString response = message.value("content").toString();
                            emit chatResponse(response);
                        }
                    } else {
                        // Fallback to previous format
                        QString response = obj.value("response").toString();
                        emit chatResponse(response);
                    }
                } else if (doc.isArray()) {
                    // Handle array response if needed
                    QJsonArray arr = doc.array();
                    QString response;
                    for (const QJsonValue &val : arr) {
                        if (val.isObject()) {
                            QJsonObject obj = val.toObject();
                            if (obj.contains("choices")) {
                                QJsonArray choices = obj.value("choices").toArray();
                                if (choices.size() > 0) {
                                    QJsonObject choice = choices[0].toObject();
                                    QJsonObject message = choice.value("message").toObject();
                                    response += message.value("content").toString() + " ";
                                }
                            } else {
                                response += obj.value("response").toString() + " ";
                            }
                        }
                    }
                    emit chatResponse(response.trimmed());
                }
            } else {
                emit chatError(QString("JSON parse error: %1").arg(parseError.errorString()));
            }
        }
    }

    reply->deleteLater();
}