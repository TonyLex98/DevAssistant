#pragma once
#include <QSet>
#include <QString>
#include <QVector>

// The search providers and the query helpers of the web search.
namespace Web {

struct SearchHit {
    QString title;
    QString url;
    QString snippet;
};

// Query helpers; they take the query already lower-cased.
bool looksLikeNewsQuery(const QString& lowerQuery);
bool looksLikeProgrammingQuery(const QString& lowerQuery);

// A news query that asks for the latest events gets the current year: an older year in it is
// replaced, a missing one is appended. `noteOut` then says what was changed. The query comes
// back simplified (single spaces) in any case.
QString normalizeRecencyQuery(const QString& rawQuery, QString* noteOut = nullptr);

// A news.google.com article link: a wrapper page, not the article itself.
bool isGoogleNewsWrapperUrl(const QString& urlText);

// The providers. Each returns at most `maxResults` hits whose URL is not in `seen` yet, and
// adds their URLs to it. Requests are synchronous and need a live SearchScope.
//
// These four set `err` when the request fails or the reply is not 2xx (Google News also when
// the feed does not parse); `seen` may be null.
QVector<SearchHit> searchGoogleNewsRss(const QString& query,
                                       int maxResults,
                                       QSet<QString>* seen,
                                       QString* err,
                                       int timeoutMs,
                                       const QString& hl,
                                       const QString& gl,
                                       const QString& ceid);

QVector<SearchHit> searchWikipedia(const QString& query,
                                   int maxResults,
                                   QSet<QString>* seen,
                                   QString* err,
                                   int timeoutMs,
                                   const QString& lang);

QVector<SearchHit> searchStackOverflow(const QString& query,
                                       int maxResults,
                                       QSet<QString>* seen,
                                       QString* err,
                                       int timeoutMs);

QVector<SearchHit> searchHackerNews(const QString& query,
                                    int maxResults,
                                    QSet<QString>* seen,
                                    QString* err,
                                    int timeoutMs);

// DuckDuckGo: the instant answer API, the lite page and the HTML page. These set `err` only
// when the request itself fails (a non-2xx reply just gives no hits), `seen` must not be null,
// and the two pages set `captcha` when DuckDuckGo answers with a CAPTCHA instead of results.
QVector<SearchHit> searchDuckInstantAnswer(const QString& query,
                                           int maxResults,
                                           QSet<QString>* seen,
                                           QString* err,
                                           int timeoutMs);

QVector<SearchHit> searchDuckLite(const QString& query,
                                  int maxResults,
                                  QSet<QString>* seen,
                                  QString* err,
                                  int timeoutMs,
                                  bool* captcha);

QVector<SearchHit> searchDuckHtml(const QString& query,
                                  int maxResults,
                                  QSet<QString>* seen,
                                  QString* err,
                                  int timeoutMs,
                                  bool* captcha);

} // namespace Web
