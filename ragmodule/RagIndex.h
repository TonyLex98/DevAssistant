#pragma once
#include "RagTypes.h"

#include <QObject>
#include <QPointer>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <functional>

namespace Rag {

// Settings for indexing and answering, read from the app's model settings and "rag/*" keys.
struct Settings
{
    QUrl endpoint;
    QString provider;          // "ollama" or an OpenAI-compatible server
    QString apiKey;
    QString embeddingModel;    // rag/embeddingModel, default nomic-embed-text
    QString chatModel;         // the model selected in the chat
    int chunkChars = 1200;     // rag/chunkChars
    int overlapChars = 200;    // rag/overlapChars
    int topK = 6;              // rag/topK

    static Settings current();
};

// The index of one folder, stored in <folder>/DevelopersAssistant/rag.sqlite.
//
// Indexing and answering run in a worker thread; the signals arrive in the thread that owns
// this object. Re-indexing only processes files that changed since the last run and drops
// files that were deleted.
class Index : public QObject
{
    Q_OBJECT
public:
    explicit Index(QObject* parent = nullptr);
    ~Index() override;

    static QString databasePathFor(const QString& folder);

    bool isBusy() const;
    void startIndexing(const QString& folder);
    void startAsking(const QString& folder, const QString& question);
    void cancel();

    // Blocking search, for the SEARCH_DOCS command: embeds the query and returns the closest
    // chunks. Fails with a reason if the folder has not been indexed.
    static QVector<Hit> search(const QString& folder, const QString& query, int k, QString* error);

    // The prompt that answers a question from the given chunks (also used by tests).
    static QString buildAnswerPrompt(const QString& question, const QVector<Hit>& hits);

signals:
    void progress(int done, int total, const QString& file);
    void indexingFinished(const QString& summary);
    void answerReady(const QString& answer, const QVector<Rag::Hit>& sources);
    void failed(const QString& error);

private:
    void runInThread(std::function<void()> work);

    QPointer<QThread> m_thread;
    std::atomic_bool m_cancel{false};
};

} // namespace Rag
