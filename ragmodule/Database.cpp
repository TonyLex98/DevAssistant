#include "Database.h"
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>

namespace Rag
{

Database::Database()
{
    mDb = QSqlDatabase::addDatabase("QSQLITE");
    // Use a persistent database file in the application's data directory
    QString dbPath = QDir::homePath() + "/.devassistant/rag_index.db";
    // Ensure the directory exists
    QFileInfo fi(dbPath);
    QDir().mkpath(fi.absolutePath());
    mDb.setDatabaseName(dbPath);
    if (!mDb.open()) {
        qWarning() << "Failed to open DB:" << mDb.lastError();
        return;
    }
    QSqlQuery q(mDb);
    q.exec("CREATE TABLE IF NOT EXISTS fragments(\n" \
           "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n" \
           "  file_path TEXT,\n" \
           "  page_number INTEGER,\n" \
           "  paragraph_number INTEGER,\n" \
           "  embedding BLOB,\n" \
           "  text TEXT\n" \
           ");");
    
    // Create table for tracking file indexing status
    q.exec("CREATE TABLE IF NOT EXISTS file_index_status(\n" \
           "  file_path TEXT PRIMARY KEY,\n" \
           "  last_modified INTEGER,\n" \
           "  indexed_at DATETIME DEFAULT CURRENT_TIMESTAMP\n" \
           ");");
}

Database::~Database()
{
    mDb.close();
}

void Database::insertFragments(const QVector<Fragment> &frags)
{
    QSqlQuery q(mDb);
    q.prepare("INSERT INTO fragments(file_path, page_number, paragraph_number, embedding, text) VALUES(?,?,?,?,?)");
    for (const Fragment &f : frags) {
        QByteArray ba; blobFromVector(f.embedding, ba);
        q.addBindValue(f.filePath);
        q.addBindValue(f.pageNumber);
        q.addBindValue(f.paragraphNumber);
        q.addBindValue(ba);
        q.addBindValue(f.text);
        if (!q.execBatch()) {
            qWarning() << "Insert error:" << q.lastError();
        }
    }
}

QVector<Fragment> Database::searchSimilar(const QVector<float> &query, int k)
{
    QVector<Fragment> result;
    QSqlQuery q(mDb);
    q.prepare("SELECT file_path, page_number, paragraph_number, embedding, text FROM fragments");
    if (!q.exec()) {
        qWarning() << "Query execution failed:" << q.lastError();
        return result;
    }
    QVector<float> maxValues;
    while (q.next()) {
        Fragment f;
        f.filePath = q.value(0).toString();
        f.pageNumber = q.value(1).toInt();
        f.paragraphNumber = q.value(2).toInt();
        f.text = q.value(4).toString();
        f.embedding = vectorFromBlob(q.value(3).toByteArray());
        float sim = cosineSimilarity(query, f.embedding);
        if (result.size() < (size_t)k) {
            result.push_back(f);
            maxValues.push_back(sim);
        } else {
            auto minIt = std::min_element(maxValues.begin(), maxValues.end());
            if (sim > *minIt) {
                size_t idx = std::distance(maxValues.begin(), minIt);
                result[idx] = f;
                *minIt = sim;
            }
        }
    }
    return result;
}

float Database::cosineSimilarity(const QVector<float> &a, const QVector<float> &b) const
{
    if (a.size() != b.size() || a.isEmpty()) return 0.0f;
    double dot = 0.0, normA = 0.0, normB = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    if (normA == 0.0 || normB == 0.0) return 0.0f;
    return static_cast<float>(dot / (std::sqrt(normA) * std::sqrt(normB)));
}

void Database::blobFromVector(const QVector<float> &v, QByteArray &ba)
{
    ba.resize(v.size() * sizeof(float));
    std::memcpy(ba.data(), v.constData(), v.size() * sizeof(float));
}

QVector<float> Database::vectorFromBlob(const QByteArray &ba)
{
    QVector<float> v;
    int count = ba.size() / sizeof(float);
    v.resize(count);
    std::memcpy(v.data(), ba.constData(), count * sizeof(float));
    return v;
}

bool Database::hasFileBeenIndexed(const QString &filePath)
{
    QSqlQuery q(mDb);
    q.prepare("SELECT last_modified FROM file_index_status WHERE file_path = ?");
    q.addBindValue(filePath);
    if (!q.exec() || !q.next()) {
        qDebug() << "File not found in index status table, marking as not indexed:" << filePath;
        return false; // File not in index status table
    }
    
    qint64 storedLastModified = q.value(0).toLongLong();
    QFileInfo fileInfo(filePath);
    QDateTime fileModified = fileInfo.lastModified();
    qint64 actualLastModified = fileModified.toMSecsSinceEpoch();
    
    qDebug() << "File:" << filePath << "Stored modified:" << storedLastModified << "Actual modified:" << actualLastModified << "Indexed:" << (storedLastModified == actualLastModified);
    
    // File is considered indexed if the stored modification time matches the actual one
    return storedLastModified == actualLastModified;
}

qint64 Database::getFileLastModified(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QDateTime fileModified = fileInfo.lastModified();
    return fileModified.toMSecsSinceEpoch();
}

void Database::updateFileIndexTime(const QString &filePath, qint64 lastModified)
{
    QSqlQuery q(mDb);
    q.prepare("INSERT OR REPLACE INTO file_index_status (file_path, last_modified) VALUES (?, ?)");
    q.addBindValue(filePath);
    q.addBindValue(lastModified);
    if (!q.exec()) {
        qWarning() << "Failed to update file index time:" << q.lastError();
    } else {
        qDebug() << "Updated file index time for:" << filePath << "timestamp:" << lastModified;
    }
}

void Database::cleanupStaleEntries(const QStringList &currentFiles)
{
    QSqlQuery q(mDb);
    // Get all files in the index status table
    q.exec("SELECT file_path FROM file_index_status");
    QStringList indexedFiles;
    while (q.next()) {
        indexedFiles << q.value(0).toString();
    }
    
    // Remove entries for files that no longer exist
    for (const QString &indexedFile : indexedFiles) {
        if (!currentFiles.contains(indexedFile)) {
            QSqlQuery delQuery(mDb);
            delQuery.prepare("DELETE FROM file_index_status WHERE file_path = ?");
            delQuery.addBindValue(indexedFile);
            delQuery.exec();
            
            // Also remove fragments associated with this file
            QSqlQuery delFragments(mDb);
            delFragments.prepare("DELETE FROM fragments WHERE file_path = ?");
            delFragments.addBindValue(indexedFile);
            delFragments.exec();
        }
    }
}

} // namespace Rag
