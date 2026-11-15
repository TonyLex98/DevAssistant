#include "LightRagClient.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

LightRagClient::LightRagClient(QObject* parent)
    : QObject(parent)
{
    m_nam.setTransferTimeout(300000);
}

void LightRagClient::setServer(const QUrl& baseUrl, const QString& apiKey)
{
    m_base = baseUrl;
    m_apiKey = apiKey.trimmed();
}

QNetworkRequest LightRagClient::request(const QString& path, const QUrlQuery* query) const
{
    QUrl url = m_base;
    QString base = url.path();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    url.setPath(base + path);
    if (query)
        url.setQuery(*query);
    QNetworkRequest req(url);
    if (!m_apiKey.isEmpty())
        req.setRawHeader("X-API-Key", m_apiKey.toUtf8());
    return req;
}

void LightRagClient::handle(QNetworkReply* reply, const QString& what,
                            std::function<void(const QJsonDocument&)> onSuccess)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, what, onSuccess]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            // FastAPI puts the reason into {"detail": ...}.
            const QJsonValue detail = QJsonDocument::fromJson(body).object().value(QStringLiteral("detail"));
            QString reason = detail.isString() ? detail.toString()
                                               : QString::fromUtf8(QJsonDocument(detail.toArray()).toJson(QJsonDocument::Compact));
            if (reason.isEmpty() || reason == QLatin1String("[]"))
                reason = reply->errorString();
            if (status == 401 || status == 403)
                reason = tr("The server needs an API key (%1).").arg(reason);
            emit failed(what, reason);
            return;
        }
        onSuccess(QJsonDocument::fromJson(body));
    });
}

void LightRagClient::checkHealth()
{
    QNetworkReply* reply = m_nam.get(request(QStringLiteral("/health")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit healthChecked(false, reply->errorString());
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject cfg = o.value(QStringLiteral("configuration")).toObject();
        QStringList details;
        for (const QString& key : {QStringLiteral("llm_model"), QStringLiteral("embedding_model"),
                                   QStringLiteral("working_directory")}) {
            const QString v = cfg.value(key).toString();
            if (!v.isEmpty())
                details << QStringLiteral("%1: %2").arg(key, v);
        }
        emit healthChecked(true, details.join(QStringLiteral(", ")));
    });
}

void LightRagClient::uploadFile(const QString& filePath)
{
    auto* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit fileUploaded(filePath, false, tr("Cannot open the file."));
        delete file;
        return;
    }
    auto* multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(QFileInfo(filePath).fileName()));
    part.setBodyDevice(file);
    file->setParent(multi);
    multi->append(part);

    QNetworkReply* reply = m_nam.post(request(QStringLiteral("/documents/upload")), multi);
    multi->setParent(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, filePath]() {
        reply->deleteLater();
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            const QString detail = o.value(QStringLiteral("detail")).toString();
            emit fileUploaded(filePath, false, detail.isEmpty() ? reply->errorString() : detail);
            return;
        }
        // {"status": "success" | "duplicated" | ..., "message": "..."}
        const QString state = o.value(QStringLiteral("status")).toString();
        emit fileUploaded(filePath, state != QLatin1String("failure"), o.value(QStringLiteral("message")).toString());
    });
}

void LightRagClient::listDocuments()
{
    handle(m_nam.get(request(QStringLiteral("/documents"))), tr("List documents"), [this](const QJsonDocument& d) {
        // {"statuses": {"processed": [...], "pending": [...], "failed": [...], ...}}
        QJsonArray all;
        const QJsonObject statuses = d.object().value(QStringLiteral("statuses")).toObject();
        for (auto it = statuses.constBegin(); it != statuses.constEnd(); ++it) {
            for (const QJsonValue& v : it.value().toArray()) {
                QJsonObject doc = v.toObject();
                if (!doc.contains(QStringLiteral("status")))
                    doc.insert(QStringLiteral("status"), it.key());
                all.append(doc);
            }
        }
        emit documentsListed(all);
    });
}

void LightRagClient::query(const QString& question, const QString& mode)
{
    QNetworkRequest req = request(QStringLiteral("/query"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body;
    body.insert(QStringLiteral("query"), question);
    body.insert(QStringLiteral("mode"), mode);
    handle(m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact)), tr("Query"),
           [this](const QJsonDocument& d) {
        emit answered(d.object().value(QStringLiteral("response")).toString());
    });
}

void LightRagClient::listGraphLabels()
{
    handle(m_nam.get(request(QStringLiteral("/graph/label/list"))), tr("Graph labels"), [this](const QJsonDocument& d) {
        QStringList labels;
        for (const QJsonValue& v : d.array())
            labels << v.toString();
        emit graphLabelsListed(labels);
    });
}

void LightRagClient::loadGraph(const QString& label, int maxDepth, int maxNodes)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("label"), label);
    q.addQueryItem(QStringLiteral("max_depth"), QString::number(maxDepth));
    q.addQueryItem(QStringLiteral("max_nodes"), QString::number(maxNodes));
    handle(m_nam.get(request(QStringLiteral("/graphs"), &q)), tr("Graph"), [this](const QJsonDocument& d) {
        const QJsonObject o = d.object();
        emit graphLoaded(o.value(QStringLiteral("nodes")).toArray(), o.value(QStringLiteral("edges")).toArray());
    });
}
