#pragma once

#include <QDateTime>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// Archived context chunk (for topic changes / context compaction).
struct ArchivedContextChunk {
    QString id;
    QDateTime timestamp;
    QString title;
    QString reason;
    QString triggerQuery;
    QStringList keywords;
    QString contextText;
};

// Context taken out of the prompt is kept here rather than dropped, so that when the user
// returns to an old subject the matching chunks can be shown to the model again.
// The current topic lives here too, because every chunk is labeled with the topic it came from.
class ContextArchive
{
public:
    // Remembers filePath so that every later change can be saved there.
    void load(const QString& filePath);
    void save() const;

    bool hasTopic() const { return !m_topicKeywords.isEmpty(); }
    bool isTopicShift(const QSet<QString>& queryKeywords) const;
    void startTopic(const QString& query, const QSet<QString>& queryKeywords);
    void extendTopic(const QSet<QString>& queryKeywords);

    // untitledTitle names the chunk when no topic has been seen yet.
    void store(const QString& contextText, const QString& untitledTitle,
               const QString& reason, const QString& triggerQuery = QString());

    // Chooses the chunks similar to the query; exhumedContextText() is what the prompt gets of them.
    void updateExhumed(const QString& queryText);
    QString exhumedContextText() const;

private:
    void prune();

    QList<ArchivedContextChunk> m_chunks;
    QStringList m_exhumedIds;
    QSet<QString> m_topicKeywords;
    QString m_topicTitle;
    QString m_filePath;
};
