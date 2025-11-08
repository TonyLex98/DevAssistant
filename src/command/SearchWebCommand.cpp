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
#include <QHash>
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

static QString fetchPageText(const QString& url, int maxChars)
{
    QUrl qurl(url);
    if (!qurl.isValid() || qurl.scheme().isEmpty())
        return QString();

    QByteArray body;
    int status = 0;
    QString err;
    if (!httpGet(qurl, &body, &status, &err, url))
        return QString();
    if (status < 200 || status >= 300)
        return QString();

    QString text = extractTextFromHtml(QString::fromUtf8(body));
    if (text.isEmpty())
        return QString();
    if (text.size() > maxChars)
        text = text.left(maxChars);
    return text;
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
        ? QString::fromLatin1("Найти в сети: \"%1\"").arg(query)
        : descr;
}

bool SearchWebCommand::apply(DevAssManager*, QString* err)
{
    m_results.clear();
    const QString query = m_params.value("query").toString().trimmed();
    qDebug() << "[SearchWebCommand] Starting search with query:" << query;
    
    if (query.isEmpty()) {
        if (err) *err = QStringLiteral("SEARCH_WEB: пустой запрос");
        qDebug() << "[SearchWebCommand] Empty query error";
        return false;
    }

    const int maxResults = qMax(1, m_params.value("maxResults").toInt(5));
    QString lastError;
    QSet<QString> seen;
    QVector<SearchHit> hits;

    try {
        qDebug() << "[SearchWebCommand] Starting warmup for query:" << query;
        warmupDuckDuckGo(query);

        // 1. Попытка через JSON API DuckDuckGo
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

        qDebug() << "[SearchWebCommand] Final hits count before processing:" << hits.size();
        if (hits.isEmpty()) {
            qDebug() << "[SearchWebCommand] No hits found, lastError:" << lastError;
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
                if (err)
                    *err = lastError.isEmpty()
                        ? QStringLiteral("SEARCH_WEB: совпадения не найдены")
                        : lastError;
                qDebug() << "[SearchWebCommand] No results and no network error, returning false";
                return false;
            }
        }

        const int maxContentChars = 2000;
        qDebug() << "[SearchWebCommand] Processing" << hits.size() << "hits into final results";
        
        QStringList articleContents;
        const int maxPagesToExtract = qMin(3, maxResults); // Только первые 3 страницы для извлечения контента
        
        for (int i = 0; i < hits.size(); ++i) {
            if (i >= maxResults)
                break;
            
            const SearchHit& hit = hits[i];
            qDebug() << "[SearchWebCommand] Processing hit" << i + 1 << ":" << hit.title << hit.url;
            if (i < maxPagesToExtract) {
                const QString article = fetchPageText(hit.url, maxContentChars);
                if (!article.isEmpty()) {
                    articleContents << article;
                    qDebug() << "[SearchWebCommand] Extracted article content for hit" << hit.title.left(40);
                }
            }
        }
        
        QStringList summaryBlocks;
        summaryBlocks << QString("=== РЕЗУЛЬТАТЫ ПОИСКА ПО ЗАПРОСУ '%1' ===").arg(query);
        
        if (!articleContents.isEmpty()) {
            summaryBlocks << QString("Суммаризация по %1 источникам:").arg(articleContents.size());
            const QString combinedContent = articleContents.join(QStringLiteral("\n\n")).trimmed();
            summaryBlocks << summarizeContentInternal(combinedContent, query);
        } else {
            summaryBlocks << QString("Найдено %1 результатов, но контент для суммаризации отсутствует.").arg(hits.size());
        }

        summaryBlocks << QString("\n=== ВСЕ НАЙДЕННЫЕ ССЫЛКИ ===");

        // Добавляем все найденные ссылки
        for (const SearchHit& hit : hits.mid(0, maxResults)) {
            QString linkBlock = hit.title + QLatin1Char('\n') + hit.url;
            if (!hit.snippet.isEmpty())
                linkBlock += QLatin1Char('\n') + hit.snippet;
            summaryBlocks << linkBlock;
        }
        
        m_results = summaryBlocks;
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
