#include "core/context/ContextArchive.h"
#include "core/context/ContextKeywords.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include "core/util/Logging.h"

namespace {
constexpr int kMaxArchiveChunks = 60;
constexpr int kMaxExhumedChunks = 2;
constexpr int kMaxTopicKeywords = 60;
constexpr double kTopicShiftThreshold = 0.12;
constexpr double kExhumeThreshold = 0.18;

// QSet::fromList is deprecated in Qt 5.15.
QSet<QString> toSet(const QStringList& list) { return QSet<QString>(list.begin(), list.end()); }

bool isTimeSensitiveNewsQuery(const QString& text)
{
    const QString lower = text.toLower();
    const bool looksLikeNews =
        lower.contains(QStringLiteral("новост")) ||
        lower.contains(QStringLiteral("украин")) ||
        lower.contains(QStringLiteral("росси")) ||
        lower.contains(QStringLiteral("news")) ||
        lower.contains(QStringLiteral("war"));
    if (!looksLikeNews)
        return false;

    return lower.contains(QStringLiteral("сегодня")) ||
           lower.contains(QStringLiteral("сейчас")) ||
           lower.contains(QStringLiteral("последн")) ||
           lower.contains(QStringLiteral("latest")) ||
           lower.contains(QStringLiteral("breaking")) ||
           lower.contains(QStringLiteral("current")) ||
           lower.contains(QStringLiteral("now"));
}

QString makeTopicTitle(const QString& text)
{
    QString t = text.trimmed();
    if (t.size() > 80)
        t = t.left(80) + QStringLiteral("...");
    return t;
}
} // namespace

void ContextArchive::load(const QString& filePath)
{
    m_filePath = filePath;
    m_chunks.clear();

    QFile f(m_filePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const QByteArray data = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return;

    const QJsonArray chunks = doc.object().value(QStringLiteral("chunks")).toArray();
    for (const QJsonValue& v : chunks) {
        const QJsonObject o = v.toObject();
        ArchivedContextChunk c;
        c.id = o.value(QStringLiteral("id")).toString();
        c.title = o.value(QStringLiteral("title")).toString();
        c.reason = o.value(QStringLiteral("reason")).toString();
        c.triggerQuery = o.value(QStringLiteral("triggerQuery")).toString();
        c.timestamp = QDateTime::fromString(o.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
        for (const QJsonValue& kw : o.value(QStringLiteral("keywords")).toArray())
            c.keywords << kw.toString();
        c.contextText = o.value(QStringLiteral("contextText")).toString();
        if (!c.id.isEmpty() && !c.contextText.trimmed().isEmpty())
            m_chunks << c;
    }

    prune();
}

void ContextArchive::save() const
{
    if (m_filePath.isEmpty())
        return;

    QJsonArray chunks;
    for (const auto& c : m_chunks) {
        QJsonObject o;
        o[QStringLiteral("id")] = c.id;
        o[QStringLiteral("timestamp")] = c.timestamp.isValid() ? c.timestamp.toString(Qt::ISODate) : QString();
        o[QStringLiteral("title")] = c.title;
        o[QStringLiteral("reason")] = c.reason;
        o[QStringLiteral("triggerQuery")] = c.triggerQuery;
        QJsonArray kws;
        for (const auto& kw : c.keywords)
            kws.append(kw);
        o[QStringLiteral("keywords")] = kws;
        o[QStringLiteral("contextText")] = c.contextText;
        chunks.append(o);
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("chunks")] = chunks;

    QSaveFile out(m_filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logCore) << "[ContextArchive] Failed to write archive file:" << m_filePath << out.errorString();
        return;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit())
        qCWarning(logCore) << "[ContextArchive] Failed to commit archive file:" << m_filePath << out.errorString();
}

bool ContextArchive::isTopicShift(const QSet<QString>& queryKeywords) const
{
    return ContextKeywords::jaccard(m_topicKeywords, queryKeywords) < kTopicShiftThreshold;
}

void ContextArchive::startTopic(const QString& query, const QSet<QString>& queryKeywords)
{
    m_topicKeywords = queryKeywords;
    m_topicTitle = makeTopicTitle(query);
}

void ContextArchive::extendTopic(const QSet<QString>& queryKeywords)
{
    QSet<QString> merged = m_topicKeywords;
    for (const auto& k : queryKeywords)
        merged.insert(k);
    if (merged.size() > kMaxTopicKeywords) {
        // Short words are dropped first: longer ones tell topics apart better.
        QList<QString> list = merged.values();
        std::sort(list.begin(), list.end(), [](const QString& a, const QString& b) {
            if (a.size() != b.size())
                return a.size() < b.size();
            return a < b;
        });
        while (list.size() > kMaxTopicKeywords)
            list.removeFirst();
        merged = toSet(list);
    }
    m_topicKeywords = merged;
}

void ContextArchive::store(const QString& contextText, const QString& untitledTitle,
                           const QString& reason, const QString& triggerQuery)
{
    if (contextText.trimmed().isEmpty())
        return;

    ArchivedContextChunk chunk;
    chunk.id = QStringLiteral("ARCH_%1").arg(QString::number(QDateTime::currentMSecsSinceEpoch()));
    chunk.timestamp = QDateTime::currentDateTime();
    chunk.title = m_topicTitle.trimmed().isEmpty() ? untitledTitle : m_topicTitle.trimmed();
    chunk.reason = reason;
    chunk.triggerQuery = triggerQuery;
    chunk.keywords = m_topicKeywords.values();
    chunk.contextText = contextText;

    m_chunks.prepend(chunk);
    prune();
    save();
}

void ContextArchive::updateExhumed(const QString& queryText)
{
    // Archived news would pass for current events, so such queries bring nothing back.
    if (isTimeSensitiveNewsQuery(queryText)) {
        m_exhumedIds.clear();
        return;
    }

    if (m_chunks.isEmpty()) {
        m_exhumedIds.clear();
        return;
    }

    const QSet<QString> qk = ContextKeywords::extract(queryText);
    if (qk.isEmpty()) {
        m_exhumedIds.clear();
        return;
    }

    struct Scored { QString id; double score; QDateTime ts; };
    QList<Scored> scored;
    scored.reserve(m_chunks.size());

    for (const auto& c : m_chunks) {
        QSet<QString> ck;
        for (const auto& kw : c.keywords)
            ck.insert(kw);
        const double sim = ContextKeywords::jaccard(qk, ck);
        if (sim >= kExhumeThreshold)
            scored.push_back({c.id, sim, c.timestamp});
    }

    std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.ts > b.ts;
    });

    QStringList ids;
    for (int i = 0; i < scored.size() && i < kMaxExhumedChunks; ++i)
        ids << scored[i].id;

    m_exhumedIds = ids;
}

QString ContextArchive::exhumedContextText() const
{
    if (m_exhumedIds.isEmpty() || m_chunks.isEmpty())
        return QString();

    QStringList blocks;
    blocks << QStringLiteral("=== RELEVANT ARCHIVED CONTEXT ===");
    int added = 0;
    for (const QString& id : m_exhumedIds) {
        auto it = std::find_if(m_chunks.cbegin(), m_chunks.cend(),
                               [&](const ArchivedContextChunk& c){ return c.id == id; });
        if (it == m_chunks.cend())
            continue;

        QString block = QStringLiteral("=== ARCHIVE: %1 (%2) ===")
                            .arg(it->title.isEmpty() ? QStringLiteral("Untitled") : it->title,
                                 it->timestamp.isValid() ? it->timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("?"));
        if (!it->reason.isEmpty())
            block += QStringLiteral("\nReason: %1").arg(it->reason);
        block += QStringLiteral("\n") + it->contextText.trimmed();

        if (block.size() > 6000)
            block = block.left(6000) + QStringLiteral("\n[...archived context truncated...]");

        blocks << block;
        if (++added >= kMaxExhumedChunks)
            break;
    }
    if (blocks.size() <= 1)
        return QString();
    return blocks.join("\n\n");
}

void ContextArchive::prune()
{
    if (m_chunks.size() <= kMaxArchiveChunks)
        return;

    std::sort(m_chunks.begin(), m_chunks.end(), [](const ArchivedContextChunk& a, const ArchivedContextChunk& b) {
        return a.timestamp > b.timestamp;
    });
    while (m_chunks.size() > kMaxArchiveChunks)
        m_chunks.removeLast();
}
