#include "core/web/WebFetch.h"

#include <QDateTime>
#include <QEventLoop>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTextCodec>
#include <QTextDocumentFragment>
#include <QTimer>
#include <functional>

namespace Web {

namespace {

// Set by SearchScope for the thread it lives in.
thread_local QNetworkAccessManager* t_manager = nullptr;

QMutex& searchMutex()
{
    static QMutex m;
    return m;
}

QNetworkAccessManager& sharedManager()
{
    Q_ASSERT(t_manager);
    return *t_manager;
}

QString randomUserAgent()
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

void applyCommonHeaders(QNetworkRequest& req, const QString& referer = QString())
{
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
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

bool executeRequest(QNetworkReply* reply,
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
            *err = QStringLiteral("SEARCH_WEB: request timed out");
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

QString extractTextFromHtml(const QString& html)
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

// Page texts by URL, shared by all searches (SearchScope runs one at a time).
QHash<QString, TimedCacheEntry<QString>> g_pageTextCache;
constexpr int kMaxPageCacheEntries = 96;

bool looksLikeNonHtml(const QString& url)
{
    const QString lower = url.toLower();
    return lower.endsWith(".pdf") || lower.endsWith(".zip") || lower.endsWith(".rar") ||
           lower.endsWith(".7z") || lower.endsWith(".exe") || lower.endsWith(".msi") ||
           lower.endsWith(".dmg") || lower.endsWith(".iso") || lower.endsWith(".apk");
}

} // namespace

SearchScope::SearchScope() : m_lock(&searchMutex())
{
    m_manager.setCookieJar(new QNetworkCookieJar(&m_manager));
    t_manager = &m_manager;
}

SearchScope::~SearchScope()
{
    t_manager = nullptr;
}

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

bool httpGet(const QUrl& url,
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

bool httpPost(const QUrl& url,
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

QString decodeHtml(const QString& fragment)
{
    return QTextDocumentFragment::fromHtml(fragment).toPlainText().simplified();
}

QString decodeHtmlBytes(const QByteArray& body, const QByteArray& contentTypeHeader)
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

QStringList fetchManyPageTexts(const QStringList& urls,
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

} // namespace Web
