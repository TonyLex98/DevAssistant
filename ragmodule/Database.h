#pragma once

/// @file Database.h
/// @brief Simple SQLite wrapper for storing embeddings.

#include "RAGProcessor.h"
#include <QVector>
#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QFileInfo>
#include <cmath>
#include <cstring>

namespace Rag
{

class Database
{
public:
    Database();
    ~Database();
    void insertFragments(const QVector<Fragment> &frags);
    QVector<Fragment> searchSimilar(const QVector<float> &query, int k = 5);
    
    // Methods for tracking indexed files
    bool hasFileBeenIndexed(const QString &filePath);
    qint64 getFileLastModified(const QString &filePath);
    void updateFileIndexTime(const QString &filePath, qint64 lastModified);
    void cleanupStaleEntries(const QStringList &currentFiles);
    
private:
    QSqlDatabase mDb;
    float cosineSimilarity(const QVector<float> &a, const QVector<float> &b) const;
    static void blobFromVector(const QVector<float> &v, QByteArray &ba);
    static QVector<float> vectorFromBlob(const QByteArray &ba);
};

} // namespace Rag
