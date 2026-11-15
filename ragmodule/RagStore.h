#pragma once
#include "RagTypes.h"

#include <QSet>
#include <QString>
#include <QVector>

namespace Rag {

// The chunk store: one SQLite file per indexed folder.
//
// A Store owns its own database connection, and Qt allows a connection to be used only from
// the thread that opened it: create a Store in the thread that uses it.
class Store
{
public:
    explicit Store(const QString& databasePath);
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool isOpen() const { return m_open; }
    QString lastError() const { return m_error; }

    // True if the file was indexed with this size, modification time and embedding model.
    bool isUpToDate(const QString& path, qint64 modifiedMs, qint64 size, const QString& model) const;
    // Replaces everything stored for the file with these chunks, in one transaction.
    bool replaceDocument(const QString& path, qint64 modifiedMs, qint64 size,
                         const QString& model, const QVector<Chunk>& chunks);
    // Forgets indexed files that are not in `existing` any more.
    int removeMissing(const QSet<QString>& existing);

    // The `k` chunks closest to `query` among chunks made with `model`, best first.
    QVector<Hit> search(const QVector<float>& query, int k, const QString& model) const;

    int documentCount() const;
    int chunkCount() const;

    static float cosine(const QVector<float>& a, const QVector<float>& b);
    static QByteArray toBlob(const QVector<float>& v);
    static QVector<float> fromBlob(const QByteArray& blob);

private:
    QString m_connection;
    bool m_open = false;
    mutable QString m_error;
};

} // namespace Rag
