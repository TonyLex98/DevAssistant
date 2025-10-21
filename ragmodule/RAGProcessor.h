#pragma once

/// @file RAGProcessor.h
/// @brief Core class for indexing documents and answering queries.
///
/// This module is designed to be dropped into an existing Qt5 application.
/// It exposes a simple API:
///  - Call indexFolder(path) to scan a directory and build the embeddings
///  - Call ask(query) to get a generated answer.
///
/// Internally it uses:
///  - External tools (pdftotext, docx2txt, tesseract) for extraction
///  - Ollama REST API for embeddings
///  - SQLite for persistence
///  - A small in‑memory cache for fast similarity search

#include <QObject>
#include <QString>
#include <QVector>
#include <QThreadPool>

namespace Rag
{
struct Fragment
{
    QString filePath;
    int pageNumber = -1;
    int paragraphNumber = -1;
    QVector<float> embedding; // 768‑dim vector
    QString text;
};

class Database;
class Encoder;

/// @brief Main interface for the RAG module.
/// @attention All heavy work is done in a worker thread to keep UI responsive.
class RAGProcessor : public QObject
{
    Q_OBJECT
public:
    explicit RAGProcessor(QObject *parent = nullptr);
    ~RAGProcessor();

    /**
     * @brief init Initializes the RAG module.
     * 
     * Checks Ollama version and pulls required model if necessary.
     * Must be called before any indexing or searching.
     */
    void init();

    /// Starts indexing of the directory. Emits progress signals.
    Q_INVOKABLE void indexFolder(const QString &rootPath);

    /// Queries the system.
    Q_INVOKABLE void ask(const QString &question);

signals:
    void progress(int processed, int total);
    void indexingFinished();
    void answered(const QString &answer);

private:
    QThreadPool mThreadPool;
    Database *mDb = nullptr;
    Encoder *mEncoder = nullptr;
    QString processPDF(const QString &filePath) const;
    QString processDOCX(const QString &filePath) const;
};

} // namespace Rag
