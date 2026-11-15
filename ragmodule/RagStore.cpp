#include "RagStore.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Rag {

Store::Store(const QString& databasePath)
    : m_connection(QStringLiteral("rag-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    QDir().mkpath(QFileInfo(databasePath).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(databasePath);
    if (!db.open()) {
        m_error = db.lastError().text();
        return;
    }
    QSqlQuery q(db);
    const bool ok =
            q.exec(QStringLiteral("PRAGMA journal_mode=WAL"))
            && q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS documents("
                                     "path TEXT PRIMARY KEY, modified INTEGER, size INTEGER, model TEXT)"))
            && q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS chunks("
                                     "id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT NOT NULL, "
                                     "ordinal INTEGER, text TEXT, embedding BLOB, model TEXT)"))
            && q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS chunks_path ON chunks(path)"));
    if (!ok) {
        m_error = q.lastError().text();
        return;
    }
    m_open = true;
}

Store::~Store()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

bool Store::isUpToDate(const QString& path, qint64 modifiedMs, qint64 size, const QString& model) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT modified, size, model FROM documents WHERE path = ?"));
    q.addBindValue(path);
    if (!q.exec() || !q.next())
        return false;
    return q.value(0).toLongLong() == modifiedMs && q.value(1).toLongLong() == size
            && q.value(2).toString() == model;
}

bool Store::replaceDocument(const QString& path, qint64 modifiedMs, qint64 size,
                            const QString& model, const QVector<Chunk>& chunks)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    if (!db.transaction()) {
        m_error = db.lastError().text();
        return false;
    }
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM chunks WHERE path = ?"));
    q.addBindValue(path);
    bool ok = q.exec();

    if (ok) {
        q.prepare(QStringLiteral("INSERT INTO chunks(path, ordinal, text, embedding, model) VALUES(?, ?, ?, ?, ?)"));
        for (const Chunk& c : chunks) {
            q.addBindValue(path);
            q.addBindValue(c.ordinal);
            q.addBindValue(c.text);
            q.addBindValue(toBlob(c.embedding));
            q.addBindValue(model);
            if (!q.exec()) {
                ok = false;
                break;
            }
        }
    }
    if (ok) {
        q.prepare(QStringLiteral("INSERT OR REPLACE INTO documents(path, modified, size, model) VALUES(?, ?, ?, ?)"));
        q.addBindValue(path);
        q.addBindValue(modifiedMs);
        q.addBindValue(size);
        q.addBindValue(model);
        ok = q.exec();
    }
    if (!ok) {
        m_error = q.lastError().text();
        db.rollback();
        return false;
    }
    return db.commit();
}

int Store::removeMissing(const QSet<QString>& existing)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    QSqlQuery list(db);
    QStringList gone;
    if (list.exec(QStringLiteral("SELECT path FROM documents"))) {
        while (list.next()) {
            const QString p = list.value(0).toString();
            if (!existing.contains(p))
                gone << p;
        }
    }
    QSqlQuery q(db);
    for (const QString& p : gone) {
        q.prepare(QStringLiteral("DELETE FROM chunks WHERE path = ?"));
        q.addBindValue(p);
        q.exec();
        q.prepare(QStringLiteral("DELETE FROM documents WHERE path = ?"));
        q.addBindValue(p);
        q.exec();
    }
    return gone.size();
}

QVector<Hit> Store::search(const QVector<float>& query, int k, const QString& model) const
{
    QVector<Hit> hits;
    if (k <= 0 || query.isEmpty())
        return hits;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT path, ordinal, text, embedding FROM chunks WHERE model = ?"));
    q.addBindValue(model);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return hits;
    }
    while (q.next()) {
        Hit h;
        h.chunk.path = q.value(0).toString();
        h.chunk.ordinal = q.value(1).toInt();
        h.chunk.text = q.value(2).toString();
        h.chunk.embedding = fromBlob(q.value(3).toByteArray());
        h.score = cosine(query, h.chunk.embedding);
        hits.push_back(h);
    }
    const int n = std::min(k, hits.size());
    std::partial_sort(hits.begin(), hits.begin() + n, hits.end(),
                      [](const Hit& a, const Hit& b) { return a.score > b.score; });
    hits.resize(n);
    return hits;
}

int Store::documentCount() const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    return q.exec(QStringLiteral("SELECT COUNT(*) FROM documents")) && q.next() ? q.value(0).toInt() : 0;
}

int Store::chunkCount() const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    return q.exec(QStringLiteral("SELECT COUNT(*) FROM chunks")) && q.next() ? q.value(0).toInt() : 0;
}

float Store::cosine(const QVector<float>& a, const QVector<float>& b)
{
    if (a.size() != b.size() || a.isEmpty())
        return 0.0f;
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (int i = 0; i < a.size(); ++i) {
        dot += double(a[i]) * b[i];
        na += double(a[i]) * a[i];
        nb += double(b[i]) * b[i];
    }
    if (na == 0.0 || nb == 0.0)
        return 0.0f;
    return float(dot / (std::sqrt(na) * std::sqrt(nb)));
}

QByteArray Store::toBlob(const QVector<float>& v)
{
    QByteArray blob(v.size() * int(sizeof(float)), Qt::Uninitialized);
    if (!v.isEmpty())
        std::memcpy(blob.data(), v.constData(), size_t(blob.size()));
    return blob;
}

QVector<float> Store::fromBlob(const QByteArray& blob)
{
    QVector<float> v(blob.size() / int(sizeof(float)));
    if (!v.isEmpty())
        std::memcpy(v.data(), blob.constData(), size_t(v.size()) * sizeof(float));
    return v;
}

} // namespace Rag
