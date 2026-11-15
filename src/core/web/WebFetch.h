#pragma once
#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <algorithm>

// Synchronous HTTP for the web search: requests wait in a local event loop, so they are meant
// for a worker thread (SearchWebCommand::runsInBackground()).
namespace Web {

// The search runs in a worker thread, a different one each time, and a QNetworkAccessManager
// may only be used in the thread that created it: each search makes its own here, and the
// request functions below use it through a thread-local pointer. Only one search runs at a
// time, because the caches are shared. Every request must be made while a scope is alive.
class SearchScope
{
public:
    SearchScope();
    ~SearchScope();

private:
    // Declared before the manager, so the lock is held for the manager's whole life.
    QMutexLocker m_lock;
    QNetworkAccessManager m_manager;
};

qint64 nowMs();

template <typename T>
struct TimedCacheEntry {
    T value;
    qint64 expiresAtMs = 0;
};

// Drops the entries closest to expiry until at most `maxEntries` are left.
template <typename T>
void trimCache(QHash<QString, TimedCacheEntry<T>>& cache, int maxEntries)
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

// GET and POST (form-encoded) with browser-like headers. Return false and set `err` when the
// request fails or times out; `status` gets the HTTP status code either way.
bool httpGet(const QUrl& url,
             QByteArray* body,
             int* status,
             QString* err,
             const QString& referer = QString(),
             int timeoutMs = 15000);

bool httpPost(const QUrl& url,
              const QByteArray& payload,
              QByteArray* body,
              int* status,
              QString* err,
              const QString& referer = QString(),
              int timeoutMs = 15000);

// The plain text of an HTML fragment, on one line.
QString decodeHtml(const QString& fragment);

// Decodes a page with the charset named in the Content-Type header or in the page itself,
// UTF-8 when there is none.
QString decodeHtmlBytes(const QByteArray& body, const QByteArray& contentTypeHeader);

// Downloads the pages, `concurrency` at a time, and returns their text (at most
// `maxCharsPerPage` characters each) in the order of `urls`; pages that failed or had no text
// are left out. Texts are cached for `cacheTtlMs` (0 turns the cache off).
QStringList fetchManyPageTexts(const QStringList& urls,
                               int maxCharsPerPage,
                               int perRequestTimeoutMs,
                               int overallTimeoutMs,
                               int concurrency,
                               int cacheTtlMs);

} // namespace Web
