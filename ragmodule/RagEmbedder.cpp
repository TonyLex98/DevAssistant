#include "RagEmbedder.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <algorithm>

namespace Rag {

namespace {

QVector<float> toVector(const QJsonArray& values)
{
    QVector<float> v;
    v.reserve(values.size());
    for (const QJsonValue& x : values)
        v.push_back(float(x.toDouble()));
    return v;
}

// Keeps any base path of the endpoint ("http://host/ollama" + "/api/embed").
QUrl withPath(const QUrl& endpoint, const QString& path)
{
    QUrl url = endpoint;
    QString base = url.path();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    url.setPath(base + path);
    return url;
}

} // namespace

Embedder::Embedder(const QUrl& endpoint, Api api, const QString& model, const QString& apiKey)
    : m_endpoint(endpoint), m_api(api), m_model(model), m_apiKey(apiKey)
{
}

Embedder::Api Embedder::apiForProvider(const QString& providerName)
{
    return providerName.trimmed().compare(QLatin1String("ollama"), Qt::CaseInsensitive) == 0
            ? Api::Ollama : Api::OpenAiCompatible;
}

QVector<QVector<float>> Embedder::embed(const QStringList& texts, QString* error) const
{
    if (texts.isEmpty())
        return {};
    if (m_model.trimmed().isEmpty()) {
        if (error) *error = QCoreApplication::translate("Rag", "No embedding model is set.");
        return {};
    }

    QJsonObject body;
    body.insert(QStringLiteral("model"), m_model);
    body.insert(QStringLiteral("input"), QJsonArray::fromStringList(texts));

    QNetworkRequest req(withPath(m_endpoint, m_api == Api::Ollama ? QStringLiteral("/api/embed")
                                                                  : QStringLiteral("/v1/embeddings")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_apiKey.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());

    QNetworkAccessManager nam;  // owned by the calling thread
    QNetworkReply* reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(180000);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        if (error) *error = QCoreApplication::translate("Rag", "The embedding request timed out.");
        return {};
    }
    const QByteArray data = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString netError = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
    reply->deleteLater();

    const QJsonObject json = QJsonDocument::fromJson(data).object();
    if (!netError.isEmpty() || status >= 400) {
        QString serverMessage = json.value(QStringLiteral("error")).toString();
        if (serverMessage.isEmpty())
            serverMessage = json.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
        if (error)
            *error = QCoreApplication::translate("Rag", "Embedding request failed: %1")
                    .arg(serverMessage.isEmpty() ? netError : serverMessage);
        return {};
    }

    QVector<QVector<float>> out;
    if (m_api == Api::Ollama) {
        for (const QJsonValue& v : json.value(QStringLiteral("embeddings")).toArray())
            out.push_back(toVector(v.toArray()));
    } else {
        const QJsonArray items = json.value(QStringLiteral("data")).toArray();
        out.resize(items.size());
        for (int i = 0; i < items.size(); ++i) {
            const QJsonObject item = items.at(i).toObject();
            const int index = item.value(QStringLiteral("index")).toInt(i);
            if (index >= 0 && index < out.size())
                out[index] = toVector(item.value(QStringLiteral("embedding")).toArray());
        }
    }
    if (out.size() != texts.size() || std::any_of(out.cbegin(), out.cend(), [](const QVector<float>& v) { return v.isEmpty(); })) {
        if (error)
            *error = QCoreApplication::translate("Rag", "The server returned %1 embeddings for %2 texts. Is %3 an embedding model?")
                    .arg(out.size()).arg(texts.size()).arg(m_model);
        return {};
    }
    return out;
}

} // namespace Rag
