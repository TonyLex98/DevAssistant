#include "core/web/SearchProviders.h"
#include "core/web/WebFetch.h"

#include <QByteArray>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>
#include <QXmlStreamReader>
#include <functional>

namespace Web {

namespace {

bool hasRecencyIntent(const QString& lowerQuery)
{
    return lowerQuery.contains(QStringLiteral("\u0441\u0435\u0433\u043e\u0434\u043d\u044f")) ||
           lowerQuery.contains(QStringLiteral("\u0441\u0435\u0439\u0447\u0430\u0441")) ||
           lowerQuery.contains(QStringLiteral("\u043f\u043e\u0441\u043b\u0435\u0434\u043d")) ||
           lowerQuery.contains(QStringLiteral("today")) ||
           lowerQuery.contains(QStringLiteral("latest")) ||
           lowerQuery.contains(QStringLiteral("breaking")) ||
           lowerQuery.contains(QStringLiteral("current")) ||
           lowerQuery.contains(QStringLiteral("now"));
}

// DuckDuckGo shows this instead of results when it takes the client for a bot.
bool isDuckCaptcha(const QString& html)
{
    return html.contains(QStringLiteral("select all squares containing a duck"), Qt::CaseInsensitive);
}

QString normalizeDuckUrl(const QString& rawUrl)
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

QVector<SearchHit> parseDuckJson(const QByteArray& body,
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

QVector<SearchHit> parseDuckLiteHtml(const QString& html,
                                     int maxResults,
                                     QSet<QString>* seen)
{
    QVector<SearchHit> hits;
    if (isDuckCaptcha(html))
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

QVector<SearchHit> parseDuckHtml(const QString& html,
                                 int maxResults,
                                 QSet<QString>* seen)
{
    QVector<SearchHit> hits;

    if (isDuckCaptcha(html))
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

bool looksLikeNewsQuery(const QString& lowerQuery)
{
    return lowerQuery.contains(QStringLiteral("\u043d\u043e\u0432\u043e\u0441\u0442")) ||
           lowerQuery.contains(QStringLiteral("\u0441\u0435\u0433\u043e\u0434\u043d\u044f")) ||
           lowerQuery.contains(QStringLiteral("\u043f\u043e\u0441\u043b\u0435\u0434\u043d")) ||
           lowerQuery.contains(QStringLiteral("\u0441\u0435\u0439\u0447\u0430\u0441")) ||
           lowerQuery.contains(QStringLiteral("latest")) ||
           lowerQuery.contains(QStringLiteral("breaking")) ||
           lowerQuery.contains(QStringLiteral("current")) ||
           lowerQuery.contains(QStringLiteral("news"));
}

bool looksLikeProgrammingQuery(const QString& lowerQuery)
{
    return lowerQuery.contains(QStringLiteral("qt")) ||
           lowerQuery.contains(QStringLiteral("c++")) ||
           lowerQuery.contains(QStringLiteral("cpp")) ||
           lowerQuery.contains(QStringLiteral("python")) ||
           lowerQuery.contains(QStringLiteral("java")) ||
           lowerQuery.contains(QStringLiteral("javascript")) ||
           lowerQuery.contains(QStringLiteral("\u043e\u0448\u0438\u0431\u043a\u0430")) ||
           lowerQuery.contains(QStringLiteral("error")) ||
           lowerQuery.contains(QStringLiteral("exception")) ||
           lowerQuery.contains(QStringLiteral("stack")) ||
           lowerQuery.contains(QStringLiteral("debug"));
}

QString normalizeRecencyQuery(const QString& rawQuery, QString* noteOut)
{
    QString normalized = rawQuery.simplified();
    const QString lower = normalized.toLower();
    if (!looksLikeNewsQuery(lower) || !hasRecencyIntent(lower))
        return normalized;

    const int currentYear = QDate::currentDate().year();
    const QString currentYearStr = QString::number(currentYear);
    const QRegularExpression yearRe(QStringLiteral("\\b(20\\d{2})\\b"));

    bool hasAnyYear = false;
    bool hasCurrentYear = false;
    bool hasOnlyOldYears = true;
    auto it = yearRe.globalMatch(normalized);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        hasAnyYear = true;
        const QString y = m.captured(1);
        if (y == currentYearStr) {
            hasCurrentYear = true;
            hasOnlyOldYears = false;
            break;
        }
    }

    if (hasAnyYear && !hasCurrentYear && hasOnlyOldYears) {
        normalized.remove(yearRe);
        normalized = normalized.simplified();
        if (!normalized.isEmpty())
            normalized += QLatin1Char(' ');
        normalized += currentYearStr;
        if (noteOut) {
            *noteOut = QStringLiteral("Query year normalized to current year %1 for recency intent.")
                           .arg(currentYearStr);
        }
        return normalized;
    }

    if (!hasAnyYear) {
        if (!normalized.isEmpty())
            normalized += QLatin1Char(' ');
        normalized += currentYearStr;
        if (noteOut) {
            *noteOut = QStringLiteral("Current year %1 appended for recency intent.")
                           .arg(currentYearStr);
        }
    }
    return normalized;
}

bool isGoogleNewsWrapperUrl(const QString& urlText)
{
    const QUrl url(urlText);
    if (!url.isValid())
        return false;
    const QString host = url.host().toLower();
    if (!host.contains(QStringLiteral("news.google.")))
        return false;
    const QString path = url.path().toLower();
    return path.contains(QStringLiteral("/rss/articles/")) || path.contains(QStringLiteral("/articles/"));
}

QVector<SearchHit> searchGoogleNewsRss(const QString& query,
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

QVector<SearchHit> searchWikipedia(const QString& query,
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

QVector<SearchHit> searchStackOverflow(const QString& query,
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

QVector<SearchHit> searchHackerNews(const QString& query,
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

QVector<SearchHit> searchDuckInstantAnswer(const QString& query,
                                           int maxResults,
                                           QSet<QString>* seen,
                                           QString* err,
                                           int timeoutMs)
{
    QUrl apiUrl(QStringLiteral("https://api.duckduckgo.com/"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), query);
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    q.addQueryItem(QStringLiteral("no_redirect"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("no_html"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("no_language"), QStringLiteral("1"));
    apiUrl.setQuery(q);

    QByteArray body;
    int status = 0;
    QString localErr;
    if (httpGet(apiUrl, &body, &status, &localErr, QStringLiteral("https://duckduckgo.com/"), timeoutMs) &&
        status >= 200 && status < 300) {
        return parseDuckJson(body, maxResults, seen);
    }
    if (err && !localErr.isEmpty())
        *err = localErr;
    return {};
}

QVector<SearchHit> searchDuckLite(const QString& query,
                                  int maxResults,
                                  QSet<QString>* seen,
                                  QString* err,
                                  int timeoutMs,
                                  bool* captcha)
{
    QUrl liteUrl(QStringLiteral("https://lite.duckduckgo.com/lite/"));
    QUrlQuery liteForm;
    liteForm.addQueryItem(QStringLiteral("q"), query);
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
                 timeoutMs) &&
        liteStatus >= 200 && liteStatus < 300) {
        const QString liteHtml = decodeHtmlBytes(liteBody, QByteArray());
        if (isDuckCaptcha(liteHtml)) {
            if (captcha)
                *captcha = true;
            return {};
        }
        return parseDuckLiteHtml(liteHtml, maxResults, seen);
    }
    if (err && !liteErr.isEmpty())
        *err = liteErr;
    return {};
}

QVector<SearchHit> searchDuckHtml(const QString& query,
                                  int maxResults,
                                  QSet<QString>* seen,
                                  QString* err,
                                  int timeoutMs,
                                  bool* captcha)
{
    QUrl htmlUrl(QStringLiteral("https://html.duckduckgo.com/html/"));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("q"), query);
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
                 timeoutMs) &&
        status >= 200 && status < 300) {
        const QString html = decodeHtmlBytes(body, QByteArray());
        if (isDuckCaptcha(html)) {
            if (captcha)
                *captcha = true;
            return {};
        }
        return parseDuckHtml(html, maxResults, seen);
    }
    if (err && !htmlErr.isEmpty())
        *err = htmlErr;
    return {};
}

} // namespace Web
