#include "SearchWebCommand.h"
#include <DevAssManager.h>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <functional>

namespace {

struct SearchHit {
    QString title;
    QString url;
    QString snippet;
};

static QString decodeHtml(const QString& fragment)
{
    return QTextDocumentFragment::fromHtml(fragment).toPlainText().simplified();
}

static QString randomUserAgent()
{
    static const char* kAgents[] = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 13_5) AppleWebKit/605.1.15 "
        "(KHTML, like Gecko) Version/17.0 Safari/605.1.15"
    };
    constexpr int count = sizeof(kAgents) / sizeof(kAgents[0]);
    const int index = QRandomGenerator::global()->bounded(count);
    return QString::fromLatin1(kAgents[index]);
}

static void applyCommonHeaders(QNetworkRequest& req, const QString& referer = QString())
{
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    req.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    req.setRawHeader("Accept-Language", "ru-RU,ru;q=0.9,en-US;q=0.7");
    req.setRawHeader("Cache-Control", "no-cache");
    req.setRawHeader("Pragma", "no-cache");
    req.setRawHeader("Connection", "keep-alive");
    req.setHeader(QNetworkRequest::UserAgentHeader, randomUserAgent());
    if (!referer.isEmpty())
        req.setRawHeader("Referer", referer.toUtf8());
}

static QNetworkAccessManager& sharedManager()
{
    static QNetworkAccessManager manager;
    static bool initialized = false;
    if (!initialized) {
        manager.setCookieJar(new QNetworkCookieJar(&manager));
        initialized = true;
    }
    return manager;
}

static bool httpGet(const QUrl& url,
                    QByteArray* body,
                    int* status,
                    QString* err,
                    const QString& referer);

static void warmupDuckDuckGo(const QString& query)
{
    static QSet<QString> warmedQueries;
    const QString key = query.left(64);
    if (warmedQueries.contains(key))
        return;
    warmedQueries.insert(key);

    QUrl warmUrl(QStringLiteral("https://duckduckgo.com/"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), query);
    warmUrl.setQuery(q);
    QByteArray body;
    int status = 0;
    QString err;
    httpGet(warmUrl, &body, &status, &err, QString());
}

static bool executeRequest(QNetworkReply* reply,
                           QByteArray* body,
                           int* status,
                           QString* err)
{
    QEventLoop loop;
    bool timedOut = false;

    QTimer timer;
    timer.setSingleShot(true);
    timer.start(15000);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        timedOut = true;
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    loop.exec();

    if (reply->isRunning())
        reply->abort();

    if (timer.isActive())
        timer.stop();

    if (status)
        *status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (timedOut) {
        if (err)
            *err = QStringLiteral("SEARCH_WEB: запрос превысил тайм-аут");
        reply->deleteLater();
        return false;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (err)
            *err = QStringLiteral("SEARCH_WEB: %1").arg(reply->errorString());
        reply->deleteLater();
        return false;
    }

    *body = reply->readAll();
    reply->deleteLater();
    return true;
}

static bool httpGet(const QUrl& url,
                    QByteArray* body,
                    int* status,
                    QString* err,
                    const QString& referer = QString())
{
    QNetworkRequest req(url);
    applyCommonHeaders(req, referer);

    auto& nam = sharedManager();
    QNetworkReply* reply = nam.get(req);
    return executeRequest(reply, body, status, err);
}

static bool httpPost(const QUrl& url,
                     const QByteArray& payload,
                     QByteArray* body,
                     int* status,
                     QString* err,
                     const QString& referer = QString())
{
    QNetworkRequest req(url);
    applyCommonHeaders(req, referer);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    auto& nam = sharedManager();
    QNetworkReply* reply = nam.post(req, payload);
    return executeRequest(reply, body, status, err);
}

static QString normalizeDuckUrl(const QString& rawUrl)
{
    QString urlStr = rawUrl.trimmed();
    if (urlStr.startsWith("//"))
        urlStr.prepend("https:");
    if (urlStr.startsWith('/'))
        urlStr.prepend("https://duckduckgo.com");

    QUrl url(urlStr);
    if (!url.isValid())
        return QString();

    if (url.host().contains("duckduckgo.com", Qt::CaseInsensitive)) {
        const QUrlQuery query(url);
        const QString uddg = query.queryItemValue("uddg");
        if (!uddg.isEmpty()) {
            QString decoded = QUrl::fromPercentEncoding(uddg.toUtf8());
            QUrl candidate(decoded);
            if (candidate.isValid())
                return candidate.toString();
            return decoded;
        }
    }
    return url.toString();
}

static QVector<SearchHit> parseDuckJson(const QByteArray& body,
                                        int maxResults,
                                        QSet<QString>* seen)
{
    QVector<SearchHit> hits;
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return hits;

    const QJsonObject root = doc.object();

    auto pushResult = [&](const QJsonObject& obj) {
        if (hits.size() >= maxResults)
            return;

        QString url = obj.value(QStringLiteral("FirstURL")).toString().trimmed();
        if (url.isEmpty())
            url = obj.value(QStringLiteral("URL")).toString().trimmed();
        QString title = obj.value(QStringLiteral("Text")).toString().trimmed();
        QString snippet = obj.value(QStringLiteral("Result")).toString();
        if (!snippet.isEmpty())
            snippet = decodeHtml(snippet);

        if (title.isEmpty() || url.isEmpty())
            return;

        if (seen->contains(url))
            return;

        seen->insert(url);
        hits.push_back({title, url, snippet});
    };

    const QJsonArray direct = root.value(QStringLiteral("Results")).toArray();
    for (const QJsonValue& v : direct) {
        if (v.isObject())
            pushResult(v.toObject());
    }

    std::function<void(const QJsonArray&)> walkTopics = [&](const QJsonArray& arr) {
        for (const QJsonValue& v : arr) {
            if (v.isObject()) {
                const QJsonObject obj = v.toObject();
                if (obj.contains(QStringLiteral("Topics"))) {
                    walkTopics(obj.value(QStringLiteral("Topics")).toArray());
                } else {
                    pushResult(obj);
                }
            }
            if (hits.size() >= maxResults)
                break;
        }
    };

    walkTopics(root.value(QStringLiteral("RelatedTopics")).toArray());

    if (hits.isEmpty()) {
        const QString abstractUrl = root.value(QStringLiteral("AbstractURL")).toString().trimmed();
        const QString abstractText = root.value(QStringLiteral("AbstractText")).toString().trimmed();
        if (!abstractUrl.isEmpty() && !abstractText.isEmpty() && !seen->contains(abstractUrl)) {
            seen->insert(abstractUrl);
            hits.push_back({abstractText, abstractUrl, QString()});
        }
    }

    return hits;
}

static QVector<SearchHit> parseDuckHtml(const QString& html,
                                        int maxResults,
                                        QSet<QString>* seen)
{
    QVector<SearchHit> hits;

    if (html.contains(QStringLiteral("select all squares containing a duck"), Qt::CaseInsensitive))
        return hits;

    const QString pattern =
        QStringLiteral("<div[^>]*class=\"[^\"]*result__body[^\"]*\"[^>]*>"
                       "[\\s\\S]*?<a[^>]*class=\"[^\"]*result__a[^\"]*\"[^>]*href=\"([^\"]+)\"[^>]*>(.*?)</a>"
                       "[\\s\\S]*?(?:<a[^>]*class=\"[^\"]*result__snippet[^\"]*\"[^>]*>(.*?)</a>)?");
    const QRegularExpression block(pattern,
                                   QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);

    auto matchIt = block.globalMatch(html);
    while (matchIt.hasNext() && hits.size() < maxResults) {
        const QRegularExpressionMatch match = matchIt.next();
        QString url = normalizeDuckUrl(match.captured(1));
        QString title = decodeHtml(match.captured(2));
        QString snippet = decodeHtml(match.captured(3));

        if (title.isEmpty() || url.isEmpty())
            continue;
        if (seen->contains(url))
            continue;

        seen->insert(url);
        hits.push_back({title, url, snippet});
    }

    return hits;
}

} // namespace

SearchWebCommand::SearchWebCommand(const QString& query,
                                   int maxResults,
                                   const QString& descr)
{
    m_params["query"] = query;
    m_params["maxResults"] = qMax(1, maxResults);
    m_description = descr.isEmpty()
        ? QString::fromLatin1("Найти в сети: \"%1\"").arg(query)
        : descr;
}

bool SearchWebCommand::apply(DevAssManager*, QString* err)
{
    m_results.clear();
    const QString query = m_params.value("query").toString().trimmed();
    if (query.isEmpty()) {
        if (err) *err = QStringLiteral("SEARCH_WEB: пустой запрос");
        return false;
    }

    const int maxResults = qMax(1, m_params.value("maxResults").toInt(5));
    QString lastError;
    QSet<QString> seen;
    QVector<SearchHit> hits;

    warmupDuckDuckGo(query);

    // 1. Попытка через JSON API DuckDuckGo
    {
        QUrl apiUrl(QStringLiteral("https://api.duckduckgo.com/"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("q"), query);
        q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
        q.addQueryItem(QStringLiteral("pretty"), QStringLiteral("1"));
        q.addQueryItem(QStringLiteral("no_redirect"), QStringLiteral("1"));
        q.addQueryItem(QStringLiteral("no_html"), QStringLiteral("1"));
        q.addQueryItem(QStringLiteral("no_language"), QStringLiteral("1"));
        apiUrl.setQuery(q);

        QByteArray body;
        int status = 0;
        if (httpGet(apiUrl, &body, &status, &lastError, QStringLiteral("https://duckduckgo.com/"))) {
            if (status == 200) {
                hits = parseDuckJson(body, maxResults, &seen);
            } else {
                lastError = QStringLiteral("SEARCH_WEB: DuckDuckGo API вернул %1").arg(status);
            }
        }
    }

    // 2. Fallback — HTML-страница
    if (hits.isEmpty()) {
        QUrl htmlUrl(QStringLiteral("https://html.duckduckgo.com/html/"));
        QUrlQuery form;
        form.addQueryItem(QStringLiteral("q"), query);
        form.addQueryItem(QStringLiteral("b"), QString());
        form.addQueryItem(QStringLiteral("kl"), QStringLiteral("ru-ru"));
        QByteArray payload = form.toString(QUrl::FullyEncoded).toUtf8();

        QByteArray body;
        int status = 0;
        QString htmlError;
        if (httpPost(htmlUrl, payload, &body, &status, &htmlError, QStringLiteral("https://html.duckduckgo.com/"))) {
            if (status == 200) {
                const QString html = QString::fromUtf8(body);
                if (html.contains(QStringLiteral("select all squares containing a duck"), Qt::CaseInsensitive)) {
                    lastError = QStringLiteral("SEARCH_WEB: DuckDuckGo запрос требует прохождения капчи");
                } else {
                    hits = parseDuckHtml(html, maxResults, &seen);
                    if (hits.isEmpty())
                        lastError = QStringLiteral("SEARCH_WEB: DuckDuckGo HTML не вернул результатов");
                }
            } else {
                lastError = QStringLiteral("SEARCH_WEB: DuckDuckGo HTML вернул %1").arg(status);
            }
        } else {
            lastError = htmlError;
        }
    }

    if (hits.isEmpty()) {
        if (err)
            *err = lastError.isEmpty()
                ? QStringLiteral("SEARCH_WEB: совпадения не найдены")
                : lastError;
        return false;
    }

    QStringList blocks;
    blocks.reserve(hits.size());
    for (const SearchHit& hit : hits.mid(0, maxResults)) {
        QString block = hit.title;
        block += QLatin1Char('\n') + hit.url;
        if (!hit.snippet.isEmpty())
            block += QLatin1Char('\n') + hit.snippet;
        blocks << block.trimmed();
    }
    m_results = blocks;
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
