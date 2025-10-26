#include "SearchWebCommand.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>

SearchWebCommand::SearchWebCommand(const QString& query,
                                   int maxResults,
                                   const QString& descr)
{
    m_params["query"] = query;
    m_params["maxResults"] = qMax(1, maxResults);
    m_description = descr.isEmpty()
        ? QString("Найти в сети: \"%1\"").arg(query)
        : descr;
}

bool SearchWebCommand::apply(DevAssManager*, QString* err)
{
    m_results.clear();
    const QString query = m_params.value("query").toString().trimmed();
    if (query.isEmpty()) {
        if (err) *err = "SEARCH_WEB: пустой запрос";
        return false;
    }
    const int maxResults = qMax(1, m_params.value("maxResults").toInt(5));
    if (!fetchDuckDuckGo(query, maxResults, m_results, err)) {
        return false;
    }
    return true;
}

bool SearchWebCommand::fetchDuckDuckGo(const QString& query,
                                       int maxResults,
                                       QStringList& out,
                                       QString* err)
{
    QUrl url("https://api.duckduckgo.com/");
    QUrlQuery q;
    q.addQueryItem("q", query);
    q.addQueryItem("format", "json");
    q.addQueryItem("no_redirect", "1");
    q.addQueryItem("no_html", "1");
    q.addQueryItem("skip_disambig", "1");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "DevAssistantQt/1.0");
    QNetworkAccessManager nam;
    QEventLoop loop;
    QNetworkReply* reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        if (err) *err = QString("SEARCH_WEB: %1").arg(reply->errorString());
        reply->deleteLater();
        return false;
    }

    QJsonParseError parseErr;
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = "SEARCH_WEB: не удалось разобрать ответ DuckDuckGo";
        return false;
    }

    const QJsonObject root = doc.object();
    auto appendTopic = [&](const QJsonObject& obj){
        if (out.size() >= maxResults) return;
        const QString text = obj.value("Text").toString().trimmed();
        const QString url = obj.value("FirstURL").toString().trimmed();
        if (text.isEmpty()) return;
        QString block = text;
        if (!url.isEmpty())
            block += "\n" + url;
        out << block;
    };

    const auto collectArray = [&](const QJsonArray& arr){
        for (const QJsonValue& v : arr) {
            if (v.isObject())
                appendTopic(v.toObject());
            if (out.size() >= maxResults)
                break;
        }
    };

    collectArray(root.value("Results").toArray());

    const QJsonArray related = root.value("RelatedTopics").toArray();
    for (const QJsonValue& v : related) {
        if (v.isObject()) {
            QJsonObject obj = v.toObject();
            if (obj.contains("Text")) {
                appendTopic(obj);
            } else if (obj.contains("Topics")) {
                collectArray(obj.value("Topics").toArray());
            }
        }
        if (out.size() >= maxResults)
            break;
    }
    return true;
}

QJsonObject SearchWebCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = m_description;
    return obj;
}
