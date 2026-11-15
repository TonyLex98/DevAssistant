#include "core/commands/SearchWebCommand.h"
#include "core/project/DevAssManager.h"
#include "core/util/Logging.h"
#include "core/web/SearchProviders.h"
#include "core/web/TextSummary.h"
#include "core/web/WebFetch.h"
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <exception>

using Web::SearchHit;

namespace {

// The hits of one search: the providers are asked in turn until there are enough hits or the
// overall deadline has passed.
class HitCollector
{
public:
    HitCollector(const QString& query, int maxResults, int searchTimeoutMs, qint64 deadlineMs)
        : m_query(query)
        , m_maxResults(maxResults)
        , m_searchTimeoutMs(searchTimeoutMs)
        , m_deadlineMs(deadlineMs)
    {
    }

    // Google News and Wikipedia, then StackOverflow or Hacker News: sources without a CAPTCHA.
    void tryNoCaptchaSources(bool looksLikeNews, bool looksLikeProgramming);
    // DuckDuckGo: the instant answer API when enabled, then the lite and the HTML pages.
    void tryDuckDuckGo(bool useInstantAnswer);

    const QVector<SearchHit>& hits() const { return m_hits; }
    // The last DuckDuckGo request error; the other providers' errors are not kept.
    const QString& lastError() const { return m_lastError; }
    bool captchaEncountered() const { return m_captchaEncountered; }

private:
    bool full() const { return m_hits.size() >= m_maxResults; }
    int wanted() const { return m_maxResults - m_hits.size(); }

    // The per-request timeout, cut to what is left before the deadline but never under half a
    // second; 0 once the deadline has passed.
    int remainingTimeoutMs() const
    {
        const qint64 remaining = m_deadlineMs - Web::nowMs();
        if (remaining <= 0)
            return 0;
        return qMax(500, qMin(m_searchTimeoutMs, static_cast<int>(remaining)));
    }

    void append(const QVector<SearchHit>& more)
    {
        for (const auto& h : more) {
            if (full())
                break;
            if (h.url.trimmed().isEmpty() || h.title.trimmed().isEmpty())
                continue;
            m_hits.push_back(h);
        }
    }

    QString m_query;
    int m_maxResults;
    int m_searchTimeoutMs;
    qint64 m_deadlineMs;
    QSet<QString> m_seen;
    QVector<SearchHit> m_hits;
    QString m_lastError;
    bool m_captchaEncountered = false;
};

void HitCollector::tryNoCaptchaSources(bool looksLikeNews, bool looksLikeProgramming)
{
    if (full())
        return;
    QString err;
    int timeout = remainingTimeoutMs();
    if (timeout <= 0)
        return;
    append(Web::searchGoogleNewsRss(m_query,
                                    wanted(),
                                    &m_seen,
                                    &err,
                                    timeout,
                                    QStringLiteral("ru"),
                                    QStringLiteral("RU"),
                                    QStringLiteral("RU:ru")));
    if (full())
        return;
    timeout = remainingTimeoutMs();
    if (timeout <= 0)
        return;
    append(Web::searchWikipedia(m_query, wanted(), &m_seen, &err, timeout, QStringLiteral("ru")));
    if (full())
        return;

    if (looksLikeNews)
        return;

    timeout = remainingTimeoutMs();
    if (timeout <= 0)
        return;
    append(Web::searchWikipedia(m_query, wanted(), &m_seen, &err, timeout, QStringLiteral("en")));
    if (full())
        return;
    timeout = remainingTimeoutMs();
    if (timeout <= 0)
        return;
    if (looksLikeProgramming)
        append(Web::searchStackOverflow(m_query, wanted(), &m_seen, &err, timeout));
    if (full())
        return;
    timeout = remainingTimeoutMs();
    if (timeout <= 0)
        return;
    if (!looksLikeProgramming)
        append(Web::searchHackerNews(m_query, wanted(), &m_seen, &err, timeout));
}

void HitCollector::tryDuckDuckGo(bool useInstantAnswer)
{
    if (full())
        return;

    if (useInstantAnswer) {
        const int timeout = remainingTimeoutMs();
        if (timeout <= 0)
            return;
        QString apiErr;
        append(Web::searchDuckInstantAnswer(m_query, wanted(), &m_seen, &apiErr, timeout));
        if (!apiErr.isEmpty())
            m_lastError = apiErr;
    }

    if (!full()) {
        const int timeout = remainingTimeoutMs();
        if (timeout <= 0)
            return;
        QString liteErr;
        append(Web::searchDuckLite(m_query, wanted(), &m_seen, &liteErr, timeout, &m_captchaEncountered));
        if (!liteErr.isEmpty())
            m_lastError = liteErr;
    }

    if (!full()) {
        const int timeout = remainingTimeoutMs();
        if (timeout <= 0)
            return;
        QString htmlErr;
        append(Web::searchDuckHtml(m_query, wanted(), &m_seen, &htmlErr, timeout, &m_captchaEncountered));
        if (!htmlErr.isEmpty())
            m_lastError = htmlErr;
    }
}

bool isNetworkError(const QString& error)
{
    return !error.isEmpty() &&
           (error.contains("TLS", Qt::CaseInsensitive) ||
            error.contains("SSL", Qt::CaseInsensitive) ||
            error.contains("certificate", Qt::CaseInsensitive) ||
            error.contains("handshake", Qt::CaseInsensitive) ||
            error.contains("network", Qt::CaseInsensitive) ||
            error.contains("connection", Qt::CaseInsensitive) ||
            error.contains("timeout", Qt::CaseInsensitive) ||
            error.contains("timed out", Qt::CaseInsensitive));
}

// The result when no provider found anything. Only with `returnDiagnostics` does it explain a
// network error, or report the failure in `errorOut`; it is still a successful result.
QStringList noHitsResult(const QString& query,
                         const QString& lastError,
                         bool returnDiagnostics,
                         QString* errorOut)
{
    QStringList result;
    result << QString("Web search result for the query: '%1'").arg(query);
    if (!returnDiagnostics) {
        result << QStringLiteral("Could not get results from the available sources.");
        result << QStringLiteral("Try refining the query or naming a source (for example: \"site:ria.ru\", \"site:bbc.com\").");
        return result;
    }

    // On a TLS, timeout or other network error, still return the diagnostics as the result.
    if (isNetworkError(lastError)) {
        result << QString("Search error: %1").arg(lastError);
        result << "Possible causes:";
        result << "- SSL/TLS certificate problems";
        result << "- Connections blocked by an antivirus/firewall";
        result << "- Network settings restrictions";
        result << "- The server did not respond in time";
        result << "";
        result << "Recommendations:";
        result << "- Check the firewall and antivirus settings";
        result << "- Make sure the internet connection is stable";
        result << "- Retry the request later";
        qCDebug(logCmd) << "[SearchWebCommand] Saved error results to m_results, count:" << result.size();
        return result;
    }

    if (errorOut)
        *errorOut = lastError.isEmpty()
            ? QStringLiteral("SEARCH_WEB: no matches found")
            : lastError;
    result << QStringLiteral("No matches found.");
    result << QStringLiteral("Try refining the query or naming a source (for example: \"site:github.com\", \"site:stackoverflow.com\").");
    qCDebug(logCmd) << "[SearchWebCommand] No results (non-network), returning informational result";
    return result;
}

// The result when the search itself threw.
QStringList exceptionResult(const QString& query, const QString& failure)
{
    QStringList result;
    result << QString("Web search result for the query: '%1'").arg(query);
    result << failure;
    result << "Possible cause: a TLS/SSL connection problem or network settings";
    return result;
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

bool SearchWebCommand::apply(DevAssManager* devAss, QString* errorOut)
{
    (void)devAss;
    Web::SearchScope scope;
    m_results.clear();
    m_rawResults.clear();
    m_summarySource.clear();
    const QString originalQuery = m_params.value("query").toString().trimmed();
    QString queryNormalizationNote;
    const QString query = Web::normalizeRecencyQuery(originalQuery, &queryNormalizationNote);
    qCDebug(logCmd) << "[SearchWebCommand] Starting search with query (with LLM):" << query
             << "original:" << originalQuery;

    if (query.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("SEARCH_WEB: empty query");
        qCDebug(logCmd) << "[SearchWebCommand] Empty query error";
        return false;
    }

    const int maxResults = qMax(1, m_params.value("maxResults").toInt(5));
    // Fast-by-default: search should return something quickly; deeper content extraction is separate.
    // Keep SEARCH_WEB fast and predictable even if the LLM suggests huge timeouts.
    const int searchTimeoutMs = qBound(900, m_params.value("searchTimeoutMs").toInt(2200), 5000);
    const int overallSearchTimeoutMs = qBound(1800, m_params.value("overallSearchTimeoutMs").toInt(5200), 9000);
    const int pageTimeoutMs = qBound(900, m_params.value("pageTimeoutMs").toInt(2200), 5000);
    const int overallPageTimeoutMs = qBound(1400, m_params.value("overallPageTimeoutMs").toInt(4200), 9000);
    const int pageConcurrency = qBound(1, m_params.value("pageConcurrency").toInt(2), 4);
    const int maxContentChars = qBound(400, m_params.value("maxContentChars").toInt(1800), 12000);
    const int cacheTtlMs = qBound(0, m_params.value("cacheTtlMs").toInt(10 * 60 * 1000), 60 * 60 * 1000);
    const int negativeCacheTtlMs = qBound(0, m_params.value("negativeCacheTtlMs").toInt(30 * 1000), 10 * 60 * 1000);
    const int maxPagesToExtractParam = m_params.value("maxPagesToExtract").toInt(3);
    const int maxPagesToExtractConfigured = qBound(0, maxPagesToExtractParam, qMin(8, maxResults));
    const bool useInstantAnswer = m_params.value("useInstantAnswer").toBool(false);
    const bool preferNoCaptchaSources = m_params.value("preferNoCaptchaSources").toBool(true);
    const QString lowerQuery = query.toLower();
    const bool looksLikeNews = Web::looksLikeNewsQuery(lowerQuery);
    const bool looksLikeProgramming = Web::looksLikeProgrammingQuery(lowerQuery);
    QString lastError;
    QVector<SearchHit> hits;

    try {
        // Hits by query and limit; a search that found nothing is kept for negativeCacheTtlMs
        // only. SearchScope runs one search at a time, so the cache needs no lock of its own.
        static QHash<QString, Web::TimedCacheEntry<QVector<SearchHit>>> queryCache;
        static constexpr int kMaxQueryCacheEntries = 48;

        const QString normalizedQuery = query.simplified();
        const QString cacheKey = QStringLiteral("%1|%2").arg(normalizedQuery, QString::number(maxResults));
        const qint64 t = Web::nowMs();
        const auto cached = queryCache.constFind(cacheKey);
        if (cached != queryCache.cend() && cached->expiresAtMs > t) {
            hits = cached->value;
        } else {
            HitCollector collector(normalizedQuery, maxResults, searchTimeoutMs, t + overallSearchTimeoutMs);
            if (preferNoCaptchaSources) {
                collector.tryNoCaptchaSources(looksLikeNews, looksLikeProgramming);
                collector.tryDuckDuckGo(useInstantAnswer);
            } else {
                collector.tryDuckDuckGo(useInstantAnswer);
                collector.tryNoCaptchaSources(looksLikeNews, looksLikeProgramming);
            }
            hits = collector.hits();
            lastError = collector.lastError();

            const int effectiveTtl = (!hits.isEmpty() ? cacheTtlMs : negativeCacheTtlMs);
            if (effectiveTtl > 0) {
                queryCache.insert(cacheKey, Web::TimedCacheEntry<QVector<SearchHit>>{hits, t + effectiveTtl});
                Web::trimCache(queryCache, kMaxQueryCacheEntries);
            }
        }

        qCDebug(logCmd) << "[SearchWebCommand] Final hits count before processing:" << hits.size();
        if (hits.isEmpty()) {
            qCDebug(logCmd) << "[SearchWebCommand] No hits found, lastError:" << lastError;
            const bool returnDiagnostics = m_params.value("returnDiagnostics").toBool(false);
            m_results = noHitsResult(query, lastError, returnDiagnostics, errorOut);
            return true;
        }

        qCDebug(logCmd) << "[SearchWebCommand] Processing" << hits.size() << "hits into final results";

        QStringList articleContents;
        QStringList snippetContents;
        for (int i = 0; i < hits.size() && i < maxResults; ++i) {
            const SearchHit& hit = hits[i];
            QString compact = hit.title.trimmed();
            if (!hit.snippet.trimmed().isEmpty())
                compact += QStringLiteral(". ") + hit.snippet.trimmed();
            if (!compact.isEmpty())
                snippetContents << compact;
        }

        const int maxPagesToExtract = looksLikeNews ? 0 : qMin(maxPagesToExtractConfigured, maxResults);
        if (maxPagesToExtract > 0) {
            QStringList urlsToExtract;
            urlsToExtract.reserve(maxPagesToExtract);
            for (int i = 0; i < hits.size() && i < maxPagesToExtract; ++i) {
                const QString& u = hits[i].url;
                if (Web::isGoogleNewsWrapperUrl(u))
                    continue;
                urlsToExtract << u;
            }

            if (!urlsToExtract.isEmpty()) {
                articleContents = Web::fetchManyPageTexts(urlsToExtract,
                                                          maxContentChars,
                                                          pageTimeoutMs,
                                                          overallPageTimeoutMs,
                                                          pageConcurrency,
                                                          cacheTtlMs);
            }
        }

        const QString combinedFromPages = articleContents.join(QStringLiteral("\n\n")).trimmed();
        const QString combinedFromSnippets = snippetContents.join(QStringLiteral("\n")).trimmed();
        const QString summarySource = !combinedFromPages.isEmpty() ? combinedFromPages : combinedFromSnippets;

        // The result with an extractive summary; the agent asks the model for a better one
        // afterwards (summarySource()), off this worker thread.
        m_summarySource = summarySource;
        QStringList resultBlocks;
        resultBlocks << QString("=== SEARCH RESULTS FOR THE QUERY '%1' ===").arg(query);

        if (!queryNormalizationNote.isEmpty())
            resultBlocks << QString("Query note: %1").arg(queryNormalizationNote);

        if (!summarySource.isEmpty()) {
            const int sourceCount = !articleContents.isEmpty() ? articleContents.size() : snippetContents.size();
            resultBlocks << QString("Summary of %1 source(s):").arg(sourceCount);
            resultBlocks << summarizeContentInternal(summarySource, query);
        } else {
            resultBlocks << QString("Found %1 result(s), but there is no content to summarize.").arg(hits.size());
        }

        resultBlocks << QString("\n=== ALL LINKS FOUND ===");

        for (const SearchHit& hit : hits.mid(0, maxResults)) {
            QString linkBlock = hit.title + QLatin1Char('\n') + hit.url;
            if (!hit.snippet.isEmpty())
                linkBlock += QLatin1Char('\n') + hit.snippet;
            resultBlocks << linkBlock;
            m_rawResults << linkBlock;
        }

        m_results = resultBlocks;
        qCDebug(logCmd) << "[SearchWebCommand] Prepared results, count:" << m_results.size();

        for (int i = 0; i < qMin(m_results.size(), 2); ++i) {  // Log first 2 results
            qCDebug(logCmd) << "  Final result" << i+1 << ":" << m_results[i].left(100) + "...";
        }
        return true;
    }
    catch (const std::exception& e) {
        // Exceptions may come from TLS errors.
        qCDebug(logCmd) << "[SearchWebCommand] Exception caught:" << QString::fromStdString(e.what());
        m_results = exceptionResult(query, QString("Request failed: %1").arg(QString::fromStdString(e.what())));
        qCDebug(logCmd) << "[SearchWebCommand] Saved exception results to m_results, count:" << m_results.size();
        return true; // succeeded, the result carries the error details
    }
    catch (...) {
        qCDebug(logCmd) << "[SearchWebCommand] Unknown exception caught";
        m_results = exceptionResult(query, QStringLiteral("Unknown error while running the search request"));
        qCDebug(logCmd) << "[SearchWebCommand] Saved unknown exception results to m_results, count:" << m_results.size();
        return true; // succeeded, the result carries the error details
    }
}

QString SearchWebCommand::summarizeContentInternal(const QString& content, const QString& query)
{
    return Web::summarizeContent(content, query);
}

QJsonObject SearchWebCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = m_description;
    return obj;
}
