#include "SearchWebCommand.h"
#include <DevAssManager.h>
#include "../adapters/OllamaAdapter.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
#include <QXmlStreamReader>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QHash>
#include <QDateTime>
#include <QTextCodec>
#include <functional>
#include <algorithm>
#include <QDebug>

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
    // Avoid compressed bodies unless we're prepared to decompress them.
    req.setRawHeader("Accept-Encoding", "identity");
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
                    const QString& referer = QString(),
                    int timeoutMs = 15000);

static bool httpPost(const QUrl& url,
                     const QByteArray& payload,
                     QByteArray* body,
                     int* status,
                     QString* err,
                     const QString& referer = QString(),
                     int timeoutMs = 15000);

static qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

template <typename T>
struct TimedCacheEntry {
    T value;
    qint64 expiresAtMs = 0;
};

template <typename T>
static void trimCache(QHash<QString, TimedCacheEntry<T>>& cache, int maxEntries)
{
    if (cache.size() <= maxEntries)
        return;

    QVector<QPair<QString, qint64>> keys;
    keys.reserve(cache.size());
    for (auto it = cache.cbegin(); it != cache.cend(); ++it)
        keys.push_back({it.key(), it.value().expiresAtMs});

    std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    const int toRemove = cache.size() - maxEntries;
    for (int i = 0; i < toRemove && i < keys.size(); ++i)
        cache.remove(keys[i].first);
}

static bool executeRequest(QNetworkReply* reply,
                           QByteArray* body,
                           int* status,
                           QString* err,
                           int timeoutMs = 15000)
{
    QEventLoop loop;
    bool timedOut = false;

    QTimer timer;
    timer.setSingleShot(true);
    timer.start(timeoutMs);
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
                    const QString& referer,
                    int timeoutMs)
{
    QNetworkRequest req(url);
    applyCommonHeaders(req, referer);

    auto& nam = sharedManager();
    QNetworkReply* reply = nam.get(req);
    return executeRequest(reply, body, status, err, timeoutMs);
}

static bool httpPost(const QUrl& url,
                     const QByteArray& payload,
                     QByteArray* body,
                     int* status,
                     QString* err,
                     const QString& referer,
                     int timeoutMs)
{
    QNetworkRequest req(url);
    applyCommonHeaders(req, referer);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    auto& nam = sharedManager();
    QNetworkReply* reply = nam.post(req, payload);
    return executeRequest(reply, body, status, err, timeoutMs);
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

static QVector<SearchHit> parseDuckLiteHtml(const QString& html,
                                            int maxResults,
                                            QSet<QString>* seen)
{
    QVector<SearchHit> hits;
    if (html.contains(QStringLiteral("select all squares containing a duck"), Qt::CaseInsensitive))
        return hits;

    const QRegularExpression linkRe(
        QStringLiteral("<a[^>]*class=\"[^\"]*result-link[^\"]*\"[^>]*href=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</a>"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression snippetRe(
        QStringLiteral("<td[^>]*class=\"[^\"]*result-snippet[^\"]*\"[^>]*>([\\s\\S]*?)</td>"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);

    auto matchIt = linkRe.globalMatch(html);
    while (matchIt.hasNext() && hits.size() < maxResults) {
        const QRegularExpressionMatch match = matchIt.next();
        QString url = normalizeDuckUrl(match.captured(1));
        QString title = decodeHtml(match.captured(2));
        if (title.isEmpty() || url.isEmpty())
            continue;
        if (seen->contains(url))
            continue;

        QString snippet;
        const int rowEnd = html.indexOf(QStringLiteral("</tr>"), match.capturedEnd(), Qt::CaseInsensitive);
        if (rowEnd > 0) {
            const QString rowChunk = html.mid(match.capturedEnd(), rowEnd - match.capturedEnd());
            const QRegularExpressionMatch s = snippetRe.match(rowChunk);
            if (s.hasMatch())
                snippet = decodeHtml(s.captured(1));
        }

        seen->insert(url);
        hits.push_back({title, url, snippet});
    }

    return hits;
}

static QVector<SearchHit> searchWikipedia(const QString& query,
                                         int maxResults,
                                         QSet<QString>* seen,
                                         QString* err,
                                         int timeoutMs,
                                         const QString& lang)
{
    QVector<SearchHit> hits;
    if (query.trimmed().isEmpty() || maxResults <= 0)
        return hits;

    QUrl url(QStringLiteral("https://%1.wikipedia.org/w/api.php").arg(lang));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    q.addQueryItem(QStringLiteral("list"), QStringLiteral("search"));
    q.addQueryItem(QStringLiteral("srsearch"), query);
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    q.addQueryItem(QStringLiteral("utf8"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("srlimit"), QString::number(maxResults));
    url.setQuery(q);

    QByteArray body;
    int status = 0;
    QString localErr;
    if (!httpGet(url, &body, &status, &localErr, QStringLiteral("https://%1.wikipedia.org/").arg(lang), timeoutMs)) {
        if (err && !localErr.isEmpty())
            *err = localErr;
        return hits;
    }
    if (status < 200 || status >= 300) {
        if (err)
            *err = QStringLiteral("SEARCH_WEB: Wikipedia returned %1").arg(status);
        return hits;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return hits;

    const QJsonObject root = doc.object();
    const QJsonObject queryObj = root.value(QStringLiteral("query")).toObject();
    const QJsonArray results = queryObj.value(QStringLiteral("search")).toArray();
    for (const QJsonValue& v : results) {
        if (hits.size() >= maxResults)
            break;
        if (!v.isObject())
            continue;
        const QJsonObject obj = v.toObject();
        const QString title = obj.value(QStringLiteral("title")).toString().trimmed();
        const QString snippetHtml = obj.value(QStringLiteral("snippet")).toString();
        const qint64 pageId = obj.value(QStringLiteral("pageid")).toVariant().toLongLong();
        if (title.isEmpty() || pageId <= 0)
            continue;

        const QString pageUrl = QStringLiteral("https://%1.wikipedia.org/?curid=%2").arg(lang, QString::number(pageId));
        if (seen && seen->contains(pageUrl))
            continue;
        if (seen)
            seen->insert(pageUrl);
        hits.push_back({title, pageUrl, decodeHtml(snippetHtml)});
    }

    return hits;
}

static QVector<SearchHit> searchStackOverflow(const QString& query,
                                              int maxResults,
                                              QSet<QString>* seen,
                                              QString* err,
                                              int timeoutMs)
{
    QVector<SearchHit> hits;
    if (query.trimmed().isEmpty() || maxResults <= 0)
        return hits;

    QUrl url(QStringLiteral("https://api.stackexchange.com/2.3/search/advanced"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("order"), QStringLiteral("desc"));
    q.addQueryItem(QStringLiteral("sort"), QStringLiteral("relevance"));
    q.addQueryItem(QStringLiteral("q"), query);
    q.addQueryItem(QStringLiteral("site"), QStringLiteral("stackoverflow"));
    q.addQueryItem(QStringLiteral("pagesize"), QString::number(maxResults));
    url.setQuery(q);

    QByteArray body;
    int status = 0;
    QString localErr;
    if (!httpGet(url, &body, &status, &localErr, QStringLiteral("https://stackoverflow.com/"), timeoutMs)) {
        if (err && !localErr.isEmpty())
            *err = localErr;
        return hits;
    }
    if (status < 200 || status >= 300) {
        if (err)
            *err = QStringLiteral("SEARCH_WEB: StackOverflow returned %1").arg(status);
        return hits;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return hits;

    const QJsonObject root = doc.object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& v : items) {
        if (hits.size() >= maxResults)
            break;
        if (!v.isObject())
            continue;
        const QJsonObject obj = v.toObject();
        const QString title = obj.value(QStringLiteral("title")).toString().trimmed();
        const QString link = obj.value(QStringLiteral("link")).toString().trimmed();
        if (title.isEmpty() || link.isEmpty())
            continue;
        if (seen && seen->contains(link))
            continue;
        if (seen)
            seen->insert(link);

        QString snippet;
        const QJsonArray tags = obj.value(QStringLiteral("tags")).toArray();
        if (!tags.isEmpty()) {
            QStringList tagList;
            for (const QJsonValue& tv : tags) {
                const QString t = tv.toString().trimmed();
                if (!t.isEmpty())
                    tagList << t;
            }
            if (!tagList.isEmpty())
                snippet = QStringLiteral("tags: %1").arg(tagList.join(QStringLiteral(", ")));
        }
        hits.push_back({title, link, snippet});
    }

    return hits;
}

static QVector<SearchHit> searchHackerNews(const QString& query,
                                           int maxResults,
                                           QSet<QString>* seen,
                                           QString* err,
                                           int timeoutMs)
{
    QVector<SearchHit> hits;
    if (query.trimmed().isEmpty() || maxResults <= 0)
        return hits;

    QUrl url(QStringLiteral("https://hn.algolia.com/api/v1/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("query"), query);
    q.addQueryItem(QStringLiteral("tags"), QStringLiteral("story"));
    q.addQueryItem(QStringLiteral("hitsPerPage"), QString::number(maxResults));
    url.setQuery(q);

    QByteArray body;
    int status = 0;
    QString localErr;
    if (!httpGet(url, &body, &status, &localErr, QStringLiteral("https://hn.algolia.com/"), timeoutMs)) {
        if (err && !localErr.isEmpty())
            *err = localErr;
        return hits;
    }
    if (status < 200 || status >= 300) {
        if (err)
            *err = QStringLiteral("SEARCH_WEB: HN Algolia returned %1").arg(status);
        return hits;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return hits;

    const QJsonObject root = doc.object();
    const QJsonArray items = root.value(QStringLiteral("hits")).toArray();
    for (const QJsonValue& v : items) {
        if (hits.size() >= maxResults)
            break;
        if (!v.isObject())
            continue;
        const QJsonObject obj = v.toObject();
        const QString title = obj.value(QStringLiteral("title")).toString().trimmed();
        QString link = obj.value(QStringLiteral("url")).toString().trimmed();
        const QString objectId = obj.value(QStringLiteral("objectID")).toString().trimmed();
        if (link.isEmpty() && !objectId.isEmpty())
            link = QStringLiteral("https://news.ycombinator.com/item?id=%1").arg(objectId);
        if (title.isEmpty() || link.isEmpty())
            continue;
        if (seen && seen->contains(link))
            continue;
        if (seen)
            seen->insert(link);

        const QString points = QString::number(obj.value(QStringLiteral("points")).toInt());
        const QString comments = QString::number(obj.value(QStringLiteral("num_comments")).toInt());
        const QString snippet = QStringLiteral("HN: %1 points, %2 comments").arg(points, comments);
        hits.push_back({title, link, snippet});
    }

    return hits;
}

static QVector<SearchHit> searchGoogleNewsRss(const QString& query,
                                              int maxResults,
                                              QSet<QString>* seen,
                                              QString* err,
                                              int timeoutMs,
                                              const QString& hl,
                                              const QString& gl,
                                              const QString& ceid)
{
    QVector<SearchHit> hits;
    if (query.trimmed().isEmpty() || maxResults <= 0)
        return hits;

    QUrl url(QStringLiteral("https://news.google.com/rss/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), query);
    q.addQueryItem(QStringLiteral("hl"), hl);
    q.addQueryItem(QStringLiteral("gl"), gl);
    q.addQueryItem(QStringLiteral("ceid"), ceid);
    url.setQuery(q);

    QByteArray body;
    int status = 0;
    QString localErr;
    if (!httpGet(url, &body, &status, &localErr, QStringLiteral("https://news.google.com/"), timeoutMs)) {
        if (err && !localErr.isEmpty())
            *err = localErr;
        return hits;
    }
    if (status < 200 || status >= 300) {
        if (err)
            *err = QStringLiteral("SEARCH_WEB: Google News RSS returned %1").arg(status);
        return hits;
    }

    QXmlStreamReader xml(body);
    while (!xml.atEnd() && hits.size() < maxResults) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        if (xml.name() != QLatin1String("item"))
            continue;

        QString title, link, description;
        while (!(xml.isEndElement() && xml.name() == QLatin1String("item")) && !xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement())
                continue;
            const QStringRef name = xml.name();
            if (name == QLatin1String("title")) {
                title = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
            } else if (name == QLatin1String("link")) {
                link = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
            } else if (name == QLatin1String("description")) {
                description = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
            }
        }

        if (title.isEmpty() || link.isEmpty())
            continue;
        if (seen && seen->contains(link))
            continue;
        if (seen)
            seen->insert(link);
        hits.push_back({title, link, decodeHtml(description)});
    }

    if (xml.hasError()) {
        if (err)
            *err = QStringLiteral("SEARCH_WEB: Google News RSS parse error");
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

static QString extractTextFromHtml(const QString& html)
{
    QString cleaned = html;
    const QRegularExpression script(R"(<script[\s\S]*?</script>)",
                                    QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression style(R"(<style[\s\S]*?</style>)",
                                   QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression noscript(R"(<noscript[\s\S]*?</noscript>)",
                                      QRegularExpression::CaseInsensitiveOption);
    cleaned.remove(script);
    cleaned.remove(style);
    cleaned.remove(noscript);
    QString text = QTextDocumentFragment::fromHtml(cleaned).toPlainText();
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    text = text.replace(QRegularExpression(QStringLiteral("[ \t]+")), QLatin1String(" "));
    text = text.replace(QRegularExpression(QStringLiteral("\n{3,}")), QLatin1String("\n\n"));
    return text.trimmed();
}

static QString decodeHtmlBytes(const QByteArray& body, const QByteArray& contentTypeHeader)
{
    auto extractCharset = [](const QByteArray& s) -> QByteArray {
        const QByteArray lower = s.toLower();
        const int idx = lower.indexOf("charset=");
        if (idx < 0)
            return QByteArray();
        int start = idx + 8;
        int end = start;
        while (end < lower.size()) {
            const char c = lower[end];
            if (c == ';' || c == '"' || c == '\'' || c == ' ' || c == '\r' || c == '\n' || c == '\t')
                break;
            ++end;
        }
        return lower.mid(start, end - start).trimmed();
    };

    QByteArray charset = extractCharset(contentTypeHeader);
    if (charset.isEmpty())
        charset = extractCharset(body.left(4096));

    if (!charset.isEmpty()) {
        QTextCodec* codec = QTextCodec::codecForName(charset);
        if (codec)
            return codec->toUnicode(body);
    }
    return QString::fromUtf8(body);
}

static QHash<QString, TimedCacheEntry<QString>> g_pageTextCache;
static constexpr int kMaxPageCacheEntries = 96;

static bool looksLikeNonHtml(const QString& url)
{
    const QString lower = url.toLower();
    return lower.endsWith(".pdf") || lower.endsWith(".zip") || lower.endsWith(".rar") ||
           lower.endsWith(".7z") || lower.endsWith(".exe") || lower.endsWith(".msi") ||
           lower.endsWith(".dmg") || lower.endsWith(".iso") || lower.endsWith(".apk");
}

static QStringList fetchManyPageTexts(const QStringList& urls,
                                      int maxCharsPerPage,
                                      int perRequestTimeoutMs,
                                      int overallTimeoutMs,
                                      int concurrency,
                                      int cacheTtlMs)
{
    QHash<QString, QString> byUrl;
    byUrl.reserve(urls.size());

    const qint64 t = nowMs();
    QStringList pending;
    pending.reserve(urls.size());

    for (const QString& url : urls) {
        const auto cached = g_pageTextCache.constFind(url);
        if (cached != g_pageTextCache.cend() && cached->expiresAtMs > t) {
            byUrl.insert(url, cached->value);
            continue;
        }
        if (!looksLikeNonHtml(url))
            pending << url;
    }

    if (!pending.isEmpty()) {
        QEventLoop loop;
        QTimer overallTimer;
        overallTimer.setSingleShot(true);
        overallTimer.start(overallTimeoutMs);
        QObject guard;

        QHash<QNetworkReply*, QString> replyToUrl;
        int nextIndex = 0;
        int finishedCount = 0;

        auto processDone = [&]() {
            if (finishedCount >= pending.size())
                loop.quit();
        };

        QObject::connect(&overallTimer, &QTimer::timeout, &guard, [&]() {
            for (auto it = replyToUrl.begin(); it != replyToUrl.end(); ++it) {
                if (it.key() && it.key()->isRunning())
                    it.key()->abort();
            }
            loop.quit();
        });

        std::function<void()> startNext = [&]() {
            while (nextIndex < pending.size() && replyToUrl.size() < qMax(1, concurrency)) {
                const QString url = pending[nextIndex++];
                QUrl qurl(url);
                if (!qurl.isValid() || qurl.scheme().isEmpty()) {
                    ++finishedCount;
                    processDone();
                    continue;
                }

                QNetworkRequest req(qurl);
                applyCommonHeaders(req, url);
                QNetworkReply* reply = sharedManager().get(req);
                replyToUrl.insert(reply, url);

                QTimer* perTimer = new QTimer(reply);
                perTimer->setSingleShot(true);
                perTimer->start(perRequestTimeoutMs);
                QObject::connect(perTimer, &QTimer::timeout, reply, &QNetworkReply::abort);

                QObject::connect(reply, &QNetworkReply::finished, &guard, [&, reply]() {
                    const QString u = replyToUrl.take(reply);
                    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if (reply->error() == QNetworkReply::NoError && httpStatus >= 200 && httpStatus < 300) {
                        const QByteArray bytes = reply->readAll();
                        const QString html = decodeHtmlBytes(bytes, reply->header(QNetworkRequest::ContentTypeHeader).toByteArray());
                        QString text = extractTextFromHtml(html);
                        if (text.size() > maxCharsPerPage)
                            text = text.left(maxCharsPerPage);
                        text = text.trimmed();
                        if (!text.isEmpty()) {
                            byUrl.insert(u, text);
                            if (cacheTtlMs > 0) {
                                g_pageTextCache.insert(u, TimedCacheEntry<QString>{text, nowMs() + cacheTtlMs});
                                trimCache(g_pageTextCache, kMaxPageCacheEntries);
                            }
                        }
                    }

                    ++finishedCount;
                    reply->deleteLater();
                    startNext();
                    processDone();
                });
            }
        };

        startNext();
        loop.exec();
    }

    QStringList out;
    out.reserve(urls.size());
    for (const QString& url : urls) {
        const QString text = byUrl.value(url).trimmed();
        if (!text.isEmpty())
            out << text;
    }
    return out;
}

static QStringList tokenizeWords(const QString& source)
{
    static const QRegularExpression wordPattern(QStringLiteral("\\b[\\p{L}0-9]+\\b"),
                                                QRegularExpression::UseUnicodePropertiesOption);
    QStringList tokens;
    auto matchIt = wordPattern.globalMatch(source.toLower());
    while (matchIt.hasNext())
        tokens << matchIt.next().captured(0);
    return tokens;
}

static QStringList splitIntoSentences(const QString& text)
{
    QStringList sentences;
    QString buffer;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (c == QLatin1Char('\r'))
            continue;
        buffer.append(c);
        if (c == '.' || c == '!' || c == '?') {
            QString trimmed = buffer.trimmed();
            if (trimmed.size() >= 5)
                sentences << trimmed;
            buffer.clear();
        }
    }
    if (!buffer.trimmed().isEmpty())
        sentences << buffer.trimmed();
    return sentences;
}

} // namespace

SearchWebCommand::SearchWebCommand(const QString& query,
                                   int maxResults,
                                   const QString& descr)
{
    m_params["query"] = query;
    m_params["maxResults"] = qMax(1, maxResults);
    m_description = descr.isEmpty()
        ? QString("Searches the web for information. "
                  "Example: { \"command\": \"SEARCH_WEB\", \"params\": { \"query\": \"Qt 6 QNetworkAccessManager example\", \"maxResults\": 3 } }")
        : descr;
    // Initialize raw results
    m_rawResults.clear();
}

bool SearchWebCommand::apply(DevAssManager* devAss, QString* err)
{
    // Вызываем applyWithLLM с пустым адаптером, чтобы использовать только классическую суммаризацию
    return applyWithLLM(devAss, nullptr, err);
}

bool SearchWebCommand::applyWithLLM(DevAssManager* devAss, void* adapter, QString* errorOut)
{
    (void)devAss;
    // Вызываем основную логику, но с возможностью использовать LLM для суммаризации
    m_results.clear();
    const QString query = m_params.value("query").toString().trimmed();
    qDebug() << "[SearchWebCommand] Starting search with query (with LLM):" << query;

    if (query.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("SEARCH_WEB: пустой запрос");
        qDebug() << "[SearchWebCommand] Empty query error";
        return false;
    }

    const int maxResults = qMax(1, m_params.value("maxResults").toInt(5));
    // Fast-by-default: search should return something quickly; deeper content extraction is separate.
    const int searchTimeoutMs = qBound(1500, m_params.value("searchTimeoutMs").toInt(4000), 20000);
    const int overallSearchTimeoutMs = qBound(2000, m_params.value("overallSearchTimeoutMs").toInt(6000), 30000);
    const int pageTimeoutMs = qBound(1500, m_params.value("pageTimeoutMs").toInt(5000), 20000);
    const int overallPageTimeoutMs = qBound(3000, m_params.value("overallPageTimeoutMs").toInt(9000), 60000);
    const int pageConcurrency = qBound(1, m_params.value("pageConcurrency").toInt(3), 8);
    const int maxContentChars = qBound(500, m_params.value("maxContentChars").toInt(2500), 20000);
    const int cacheTtlMs = qBound(0, m_params.value("cacheTtlMs").toInt(10 * 60 * 1000), 60 * 60 * 1000);
    const int negativeCacheTtlMs = qBound(0, m_params.value("negativeCacheTtlMs").toInt(30 * 1000), 10 * 60 * 1000);
    const int maxPagesToExtractParam = m_params.value("maxPagesToExtract").toInt(3);
    const int maxPagesToExtractConfigured = qBound(0, maxPagesToExtractParam, qMin(8, maxResults));
    const bool useInstantAnswer = m_params.value("useInstantAnswer").toBool(false);
    const bool preferNoCaptchaSources = m_params.value("preferNoCaptchaSources").toBool(true);
    QString lastError;
    bool captchaEncountered = false;
    QSet<QString> seen;
    QVector<SearchHit> hits;

    try {
        // 1. Попытка через JSON API DuckDuckGo
        static QHash<QString, TimedCacheEntry<QVector<SearchHit>>> queryCache;
        static constexpr int kMaxQueryCacheEntries = 48;

        const QString normalizedQuery = query.simplified();
        const QString cacheKey = QStringLiteral("%1|%2").arg(normalizedQuery, QString::number(maxResults));
        const qint64 t = nowMs();
        const auto cached = queryCache.constFind(cacheKey);
        if (cached != queryCache.cend() && cached->expiresAtMs > t) {
            hits = cached->value;
            for (const auto& h : hits)
                seen.insert(h.url);
        } else {
            const qint64 deadlineMs = t + overallSearchTimeoutMs;
            auto remainingTimeoutMs = [&]() -> int {
                const qint64 remaining = deadlineMs - nowMs();
                if (remaining <= 0)
                    return 0;
                return qMax(500, qMin(searchTimeoutMs, static_cast<int>(remaining)));
            };

            auto appendHits = [&](const QVector<SearchHit>& more) {
                for (const auto& h : more) {
                    if (hits.size() >= maxResults)
                        break;
                    if (h.url.trimmed().isEmpty() || h.title.trimmed().isEmpty())
                        continue;
                    hits.push_back(h);
                }
            };

            const QString lowerQuery = normalizedQuery.toLower();
            const bool looksLikeNews =
                lowerQuery.contains(QStringLiteral("новост")) ||
                lowerQuery.contains(QStringLiteral("сегодня")) ||
                lowerQuery.contains(QStringLiteral("latest")) ||
                lowerQuery.contains(QStringLiteral("breaking")) ||
                lowerQuery.contains(QStringLiteral("news"));
            const bool looksLikeProgramming =
                lowerQuery.contains(QStringLiteral("qt")) ||
                lowerQuery.contains(QStringLiteral("c++")) ||
                lowerQuery.contains(QStringLiteral("cpp")) ||
                lowerQuery.contains(QStringLiteral("python")) ||
                lowerQuery.contains(QStringLiteral("java")) ||
                lowerQuery.contains(QStringLiteral("javascript")) ||
                lowerQuery.contains(QStringLiteral("ошибка")) ||
                lowerQuery.contains(QStringLiteral("error")) ||
                lowerQuery.contains(QStringLiteral("exception")) ||
                lowerQuery.contains(QStringLiteral("stack")) ||
                lowerQuery.contains(QStringLiteral("debug"));

            auto tryNoCaptcha = [&]() {
                if (hits.size() >= maxResults)
                    return;
                QString err;
                int timeout = remainingTimeoutMs();
                if (timeout <= 0)
                    return;
                appendHits(searchGoogleNewsRss(normalizedQuery,
                                             maxResults - hits.size(),
                                             &seen,
                                             &err,
                                             timeout,
                                             QStringLiteral("ru"),
                                             QStringLiteral("RU"),
                                             QStringLiteral("RU:ru")));
                if (hits.size() >= maxResults)
                    return;
                timeout = remainingTimeoutMs();
                if (timeout <= 0)
                    return;
                appendHits(searchWikipedia(normalizedQuery, maxResults - hits.size(), &seen, &err, timeout, QStringLiteral("ru")));
                if (hits.size() >= maxResults)
                    return;

                if (looksLikeNews)
                    return;

                timeout = remainingTimeoutMs();
                if (timeout <= 0)
                    return;
                appendHits(searchWikipedia(normalizedQuery, maxResults - hits.size(), &seen, &err, timeout, QStringLiteral("en")));
                if (hits.size() >= maxResults)
                    return;
                timeout = remainingTimeoutMs();
                if (timeout <= 0)
                    return;
                if (looksLikeProgramming) {
                    appendHits(searchStackOverflow(normalizedQuery, maxResults - hits.size(), &seen, &err, timeout));
                }
                if (hits.size() >= maxResults)
                    return;
                timeout = remainingTimeoutMs();
                if (timeout <= 0)
                    return;
                if (!looksLikeProgramming) {
                    appendHits(searchHackerNews(normalizedQuery, maxResults - hits.size(), &seen, &err, timeout));
                }
            };

            auto tryDuck = [&]() {
                if (hits.size() >= maxResults)
                    return;

                if (useInstantAnswer && hits.size() < maxResults) {
                    const int timeout = remainingTimeoutMs();
                    if (timeout <= 0)
                        return;
                    QUrl apiUrl(QStringLiteral("https://api.duckduckgo.com/"));
                    QUrlQuery q;
                    q.addQueryItem(QStringLiteral("q"), normalizedQuery);
                    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
                    q.addQueryItem(QStringLiteral("no_redirect"), QStringLiteral("1"));
                    q.addQueryItem(QStringLiteral("no_html"), QStringLiteral("1"));
                    q.addQueryItem(QStringLiteral("no_language"), QStringLiteral("1"));
                    apiUrl.setQuery(q);

                    QByteArray body;
                    int status = 0;
                    QString apiErr;
                    if (httpGet(apiUrl, &body, &status, &apiErr, QStringLiteral("https://duckduckgo.com/"), timeout) &&
                        status >= 200 && status < 300) {
                        appendHits(parseDuckJson(body, maxResults - hits.size(), &seen));
                    } else if (!apiErr.isEmpty()) {
                        lastError = apiErr;
                    }
                }

                if (hits.size() < maxResults) {
                    const int timeout = remainingTimeoutMs();
                    if (timeout <= 0)
                        return;
                    QUrl liteUrl(QStringLiteral("https://lite.duckduckgo.com/lite/"));
                    QUrlQuery liteForm;
                    liteForm.addQueryItem(QStringLiteral("q"), normalizedQuery);
                    QByteArray litePayload = liteForm.toString(QUrl::FullyEncoded).toUtf8();

                    QByteArray liteBody;
                    int liteStatus = 0;
                    QString liteErr;
                    if (httpPost(liteUrl,
                                 litePayload,
                                 &liteBody,
                                 &liteStatus,
                                 &liteErr,
                                 QStringLiteral("https://lite.duckduckgo.com/lite/"),
                                 timeout) &&
                        liteStatus >= 200 && liteStatus < 300) {
                        const QString liteHtml = decodeHtmlBytes(liteBody, QByteArray());
                        if (liteHtml.contains(QStringLiteral("select all squares containing a duck"), Qt::CaseInsensitive)) {
                            captchaEncountered = true;
                        } else {
                            appendHits(parseDuckLiteHtml(liteHtml, maxResults - hits.size(), &seen));
                        }
                    } else if (!liteErr.isEmpty()) {
                        lastError = liteErr;
                    }
                }

                if (hits.size() < maxResults) {
                    const int timeout = remainingTimeoutMs();
                    if (timeout <= 0)
                        return;
                    QUrl htmlUrl(QStringLiteral("https://html.duckduckgo.com/html/"));
                    QUrlQuery form;
                    form.addQueryItem(QStringLiteral("q"), normalizedQuery);
                    form.addQueryItem(QStringLiteral("b"), QString());
                    form.addQueryItem(QStringLiteral("kl"), QStringLiteral("ru-ru"));
                    QByteArray payload = form.toString(QUrl::FullyEncoded).toUtf8();

                    QByteArray body;
                    int status = 0;
                    QString htmlErr;
                    if (httpPost(htmlUrl,
                                 payload,
                                 &body,
                                 &status,
                                 &htmlErr,
                                 QStringLiteral("https://html.duckduckgo.com/"),
                                 timeout) &&
                        status >= 200 && status < 300) {
                        const QString html = decodeHtmlBytes(body, QByteArray());
                        if (html.contains(QStringLiteral("select all squares containing a duck"), Qt::CaseInsensitive)) {
                            captchaEncountered = true;
                        } else {
                            appendHits(parseDuckHtml(html, maxResults - hits.size(), &seen));
                        }
                    } else if (!htmlErr.isEmpty()) {
                        lastError = htmlErr;
                    }
                }
            };

            if (preferNoCaptchaSources) {
                tryNoCaptcha();
                tryDuck();
            } else {
                tryDuck();
                tryNoCaptcha();
            }

            const int effectiveTtl = (!hits.isEmpty() ? cacheTtlMs : negativeCacheTtlMs);
            if (effectiveTtl > 0) {
                queryCache.insert(cacheKey, TimedCacheEntry<QVector<SearchHit>>{hits, t + effectiveTtl});
                trimCache(queryCache, kMaxQueryCacheEntries);
            }
        }

#if 0
        qDebug() << "[SearchWebCommand] Attempting JSON API request";
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

            qDebug() << "[SearchWebCommand] API URL:" << apiUrl.toString();
            QByteArray body;
            int status = 0;
            if (httpGet(apiUrl, &body, &status, &lastError, QStringLiteral("https://duckduckgo.com/"))) {
                qDebug() << "[SearchWebCommand] API request successful, status:" << status;
                if (status >= 200 && status < 300) {  // Accept any 2xx status code as success
                    hits = parseDuckJson(body, maxResults, &seen);
                    qDebug() << "[SearchWebCommand] API results count:" << hits.size();
                    for (int i = 0; i < qMin(hits.size(), 3); ++i) {  // Log first 3 hits
                        qDebug() << "  Hit" << i+1 << ":" << hits[i].title << "at" << hits[i].url;
                    }
                } else {
                    lastError = QStringLiteral("SEARCH_WEB: DuckDuckGo API вернул %1").arg(status);
                    qDebug() << "[SearchWebCommand] API request failed with status:" << status;
                }
            } else {
                qDebug() << "[SearchWebCommand] API request failed with error:" << lastError;
                qDebug() << "[SearchWebCommand] API request error details: HTTP request failed at network level";
            }
        }

        // 2. Fallback — HTML-страница
        if (hits.isEmpty()) {
            qDebug() << "[SearchWebCommand] API returned no results, trying HTML fallback";
            QUrl htmlUrl(QStringLiteral("https://html.duckduckgo.com/html/"));
            QUrlQuery form;
            form.addQueryItem(QStringLiteral("q"), query);
            form.addQueryItem(QStringLiteral("b"), QString());
            form.addQueryItem(QStringLiteral("kl"), QStringLiteral("ru-ru"));
            QByteArray payload = form.toString(QUrl::FullyEncoded).toUtf8();

            qDebug() << "[SearchWebCommand] HTML URL:" << htmlUrl.toString();
            QByteArray body;
            int status = 0;
            QString htmlError;
            if (httpPost(htmlUrl, payload, &body, &status, &htmlError, QStringLiteral("https://html.duckduckgo.com/"))) {
                qDebug() << "[SearchWebCommand] HTML request successful, status:" << status;
                if (status >= 200 && status < 300) {
                    const QString html = QString::fromUtf8(body);
                    if (html.contains(QStringLiteral("select all squares containing a duck"), Qt::CaseInsensitive)) {
                        lastError = QStringLiteral("SEARCH_WEB: DuckDuckGo запрос требует прохождения капчи");
                        qDebug() << "[SearchWebCommand] CAPTCHA required";
                    } else {
                        hits = parseDuckHtml(html, maxResults, &seen);
                        qDebug() << "[SearchWebCommand] HTML parsing results count:" << hits.size();
                        for (int i = 0; i < qMin(hits.size(), 3); ++i) {  // Log first 3 hits
                            qDebug() << "  HTML Hit" << i+1 << ":" << hits[i].title << "at" << hits[i].url;
                        }
                        if (hits.isEmpty()) {
                            lastError = QStringLiteral("SEARCH_WEB: DuckDuckGo HTML не вернул результатов");
                            qDebug() << "[SearchWebCommand] HTML parsing returned no results";
                        } else {
                            qDebug() << "[SearchWebCommand] Successfully parsed HTML results";
                        }
                    }
                } else {
                    lastError = QStringLiteral("SEARCH_WEB: DuckDuckGo HTML вернул %1").arg(status);
                    qDebug() << "[SearchWebCommand] HTML request failed with status:" << status;
                }
            } else {
                lastError = htmlError;
                qDebug() << "[SearchWebCommand] HTML request failed with error:" << htmlError;
            }
        }

#endif

        qDebug() << "[SearchWebCommand] Final hits count before processing:" << hits.size();
        if (hits.isEmpty()) {
            qDebug() << "[SearchWebCommand] No hits found, lastError:" << lastError;
            const bool returnDiagnostics = m_params.value("returnDiagnostics").toBool(false);
            if (!returnDiagnostics) {
                QStringList msg;
                msg << QString("Результат поиска в интернете по запросу: '%1'").arg(query);
                msg << QStringLiteral("Не удалось получить результаты из доступных источников.");
                msg << QStringLiteral("Попробуйте уточнить запрос или указать источник (например: \"site:ria.ru\", \"site:bbc.com\").");
                m_results = msg;
                return true;
            }
            // Если была TLS ошибка, таймаут или другая ошибка сети, всё равно сохранить информацию
            if (!lastError.isEmpty() &&
                (lastError.contains("TLS", Qt::CaseInsensitive) ||
                 lastError.contains("SSL", Qt::CaseInsensitive) ||
                 lastError.contains("certificate", Qt::CaseInsensitive) ||
                 lastError.contains("handshake", Qt::CaseInsensitive) ||
                 lastError.contains("network", Qt::CaseInsensitive) ||
                 lastError.contains("connection", Qt::CaseInsensitive) ||
                 lastError.contains("timeout", Qt::CaseInsensitive) ||
                 lastError.contains("превысил тайм-аут", Qt::CaseInsensitive) ||
                 lastError.contains("timed out", Qt::CaseInsensitive))) {

                // Сохраняем информацию об ошибке как результат
                QStringList errorResult;
                errorResult << QString("Результат поиска в интернете по запросу: '%1'").arg(query);
                errorResult << QString("Ошибка поиска: %1").arg(lastError);
                errorResult << "Возможные причины:";
                errorResult << "- Проблемы с SSL/TLS сертификатами";
                errorResult << "- Блокировка соединений антивирусом/брандмауэром";
                errorResult << "- Ограничения сетевых настроек";
                errorResult << "- Превышение времени ожидания ответа от сервера";
                errorResult << "";
                errorResult << "Рекомендации:";
                errorResult << "- Проверить настройки брандмауэра и антивируса";
                errorResult << "- Убедиться в наличии стабильного доступа к интернету";
                errorResult << "- Повторить запрос позже";

                m_results = errorResult;
                qDebug() << "[SearchWebCommand] Saved error results to m_results, count:" << m_results.size();
                return true; // Успешно выполнено с информацией об ошибке
            } else {
                if (errorOut)
                    *errorOut = lastError.isEmpty()
                        ? QStringLiteral("SEARCH_WEB: совпадения не найдены")
                        : lastError;
                QStringList noResults;
                noResults << QString("Результат поиска в интернете по запросу: '%1'").arg(query);
                noResults << QStringLiteral("Совпадения не найдены.");
                noResults << QStringLiteral("Попробуйте уточнить запрос или указать источник (например: \"site:github.com\", \"site:stackoverflow.com\").");
                m_results = noResults;
                qDebug() << "[SearchWebCommand] No results (non-network), returning informational result";
                return true;
            }
        }

        qDebug() << "[SearchWebCommand] Processing" << hits.size() << "hits into final results";

        QStringList articleContents;
        const int maxPagesToExtract = qMin(maxPagesToExtractConfigured, maxResults);

        if (maxPagesToExtract > 0) {
            QStringList urlsToExtract;
            urlsToExtract.reserve(maxPagesToExtract);
            for (int i = 0; i < hits.size() && i < maxPagesToExtract; ++i)
                urlsToExtract << hits[i].url;

            articleContents = fetchManyPageTexts(urlsToExtract,
                                                 maxContentChars,
                                                 pageTimeoutMs,
                                                 overallPageTimeoutMs,
                                                 pageConcurrency,
                                                 cacheTtlMs);
        }

        // Подготовка результата - если доступен LLM, используем его для суммаризации
        QStringList resultBlocks;
        resultBlocks << QString("=== РЕЗУЛЬТАТЫ ПОИСКА ПО ЗАПРОСУ '%1' ===").arg(query);

        if (!articleContents.isEmpty() && adapter) {
            // Используем LLM для суммаризации
            QString combinedContent = articleContents.join(QStringLiteral("\n\n")).trimmed();
            QString llmSummary = generateLLMSummary(adapter, combinedContent, query);
            resultBlocks << QString("LLM-суммаризация по %1 источникам:").arg(articleContents.size());
            resultBlocks << llmSummary;
        } else if (!articleContents.isEmpty()) {
            // Используем внутреннюю суммаризацию если LLM недоступен
            resultBlocks << QString("Суммаризация по %1 источникам:").arg(articleContents.size());
            const QString combinedContent = articleContents.join(QStringLiteral("\n\n")).trimmed();
            resultBlocks << summarizeContentInternal(combinedContent, query);
        } else {
            resultBlocks << QString("Найдено %1 результатов, но контент для суммаризации отсутствует.").arg(hits.size());
        }

        resultBlocks << QString("\n=== ВСЕ НАЙДЕННЫЕ ССЫЛКИ ===");

        // Добавляем все найденные ссылки
        for (const SearchHit& hit : hits.mid(0, maxResults)) {
            QString linkBlock = hit.title + QLatin1Char('\n') + hit.url;
            if (!hit.snippet.isEmpty())
                linkBlock += QLatin1Char('\n') + hit.snippet;
            resultBlocks << linkBlock;
        }

        m_results = resultBlocks;
        qDebug() << "[SearchWebCommand] Prepared results, count:" << m_results.size();

        for (int i = 0; i < qMin(m_results.size(), 2); ++i) {  // Log first 2 results
            qDebug() << "  Final result" << i+1 << ":" << m_results[i].left(100) + "...";
        }
        return true;
    }
    catch (const std::exception& e) {
        // Обработка исключений, которые могут возникнуть при TLS ошибках
        qDebug() << "[SearchWebCommand] Exception caught:" << QString::fromStdString(e.what());
        QStringList errorResult;
        errorResult << QString("Результат поиска в интернете по запросу: '%1'").arg(query);
        errorResult << QString("Ошибка выполнения запроса: %1").arg(QString::fromStdString(e.what()));
        errorResult << "Возможная причина: проблема с TLS/SSL соединением или сетевыми настройками";
        m_results = errorResult;
        qDebug() << "[SearchWebCommand] Saved exception results to m_results, count:" << m_results.size();
        return true; // Успешно выполнено, но с информацией об ошибке
    }
    catch (...) {
        // Обработка всех остальных исключений
        qDebug() << "[SearchWebCommand] Unknown exception caught";
        QStringList errorResult;
        errorResult << QString("Результат поиска в интернете по запросу: '%1'").arg(query);
        errorResult << "Неизвестная ошибка при выполнении поискового запроса";
        errorResult << "Возможная причина: проблема с TLS/SSL соединением или сетевыми настройками";
        m_results = errorResult;
        qDebug() << "[SearchWebCommand] Saved unknown exception results to m_results, count:" << m_results.size();
        return true; // Успешно выполнено, но с информацией об ошибке
    }
}

QString SearchWebCommand::summarizeContentInternal(const QString& content, const QString& query)
{
    const QString trimmedContent = content.trimmed();
    if (trimmedContent.isEmpty())
        return QStringLiteral("Текст для суммаризации отсутствует.");

    QStringList sentences = splitIntoSentences(trimmedContent);
    if (sentences.isEmpty())
        return trimmedContent.left(1000);

    QStringList queryTerms = tokenizeWords(query);
    queryTerms.removeDuplicates();

    struct SentenceScore {
        QString sentence;
        double score;
        int index;
    };

    QVector<SentenceScore> scored;
    scored.reserve(sentences.size());

    for (int index = 0; index < sentences.size(); ++index) {
        const QString& sentence = sentences[index];
        QStringList words = tokenizeWords(sentence);
        if (words.isEmpty())
            continue;

        QHash<QString, int> frequency;
        frequency.reserve(words.size());
        for (const QString& word : words)
            frequency[word]++;

        double score = 0.0;
        if (!queryTerms.isEmpty()) {
            for (const QString& term : queryTerms) {
                auto it = frequency.find(term);
                if (it != frequency.end())
                    score += it.value() * 2.5;
            }
            score += std::min(words.size(), 40) * 0.08;
        } else {
            score = words.size();
        }

        scored.append({sentence, score, index});
    }

    if (scored.isEmpty())
        return sentences.first();

    std::sort(scored.begin(), scored.end(), [](const SentenceScore& a, const SentenceScore& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.index < b.index;
    });

    const int maxSentences = std::min(3, static_cast<int>(scored.size()));
    QVector<SentenceScore> summarySentences;
    summarySentences.reserve(maxSentences);
    for (int i = 0; i < maxSentences; ++i)
        summarySentences.append(scored[i]);

    std::sort(summarySentences.begin(), summarySentences.end(), [](const SentenceScore& a, const SentenceScore& b) {
        return a.index < b.index;
    });

    QStringList summaryParts;
    for (const auto& info : summarySentences)
        summaryParts << info.sentence.trimmed();

    QString summary = summaryParts.join(" ");
    if (summary.isEmpty())
        summary = sentences.first();

    const QString effectiveQuery = query.trimmed().isEmpty() ? QStringLiteral("поисковый запрос") : query.trimmed();
    return QString("Суммаризация по запросу '%1':\n%2").arg(effectiveQuery, summary.trimmed());
}

QJsonObject SearchWebCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = m_description;
    return obj;
}

QString SearchWebCommand::generateLLMSummary(void* adapter, const QString& content, const QString& query)
{
    if (!adapter) {
        return QStringLiteral("Ошибка: OllamaAdapter не предоставлен для суммаризации.");
    }

    // Приведение void* к OllamaAdapter*
    OllamaAdapter* ollamaAdapter = static_cast<OllamaAdapter*>(adapter);

    if (ollamaAdapter->selectedModel().isEmpty()) {
        return QStringLiteral("LLM модель не выбрана, суммаризация невозможна.");
    }

    QString effectiveQuery = query.trimmed();
    if (effectiveQuery.isEmpty())
        effectiveQuery = QStringLiteral("поисковый запрос без текста");

    QString contentForPrompt = content.trimmed();
    QString truncationNote;
    constexpr int kMaxContextChars = 12000;
    if (contentForPrompt.size() > kMaxContextChars) {
        contentForPrompt = contentForPrompt.left(kMaxContextChars);
        truncationNote = QStringLiteral("\n[Контент обрезан до %1 символов]").arg(kMaxContextChars);
    }

    const QString systemPrompt = QStringLiteral(
        "Ты ассистент-разработчика. Сформируй на основе найденного обзора краткую, структурированную суммаризацию. "
        "Укажи ключевые выводы, ссылки на источники и рекомендуемые действия. Отвечай на русском языке, не выдумывай фактов."
    );
    const QString userPrompt = QStringLiteral(
        "Запрос пользователя: %1\n"
        "Найденный контент для суммаризации:\n%2%3\n\n"
        "Сформируй итоговое резюме (3–4 тезиса или коротких абзаца) с указанием ключевых фактов, ссылок и возможных действий."
    ).arg(effectiveQuery, contentForPrompt, truncationNote);

    QString summary = ollamaAdapter->generateWithSystem(userPrompt, systemPrompt).trimmed();
    const QString errorPrefix = QStringLiteral("[Ollama error]");
    if (summary.startsWith(errorPrefix)) {
        return QStringLiteral("LLM вернула ошибку: %1").arg(summary.mid(errorPrefix.length()).trimmed());
    } else if (summary.isEmpty()) {
        return QStringLiteral("LLM не вернула ответ на суммаризацию.");
    }

    return summary;
}
