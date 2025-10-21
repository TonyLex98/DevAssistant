#include "Database.h"
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Rag
{

Database::Database()
{
    mDb = QSqlDatabase::addDatabase("QSQLITE");
    mDb.setDatabaseName(":memory:"); // In‑memory database; change to file path if persistence needed
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
    QSqlQuery q(mDb, "SELECT file_path, page_number, paragraph_number, embedding, text FROM fragments");
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

} // namespace Rag
