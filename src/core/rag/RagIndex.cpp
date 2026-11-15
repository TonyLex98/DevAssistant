#include "core/rag/RagIndex.h"

#include "core/rag/RagEmbedder.h"
#include "core/rag/RagStore.h"
#include "core/rag/RagText.h"
#include "core/model/LlmClient.h"
#include "core/util/AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QSettings>

namespace Rag {

namespace {

bool isSkippedPath(const QString& path)
{
    static const QStringList skipped = {
        "/.git/", "/node_modules/", "/DevelopersAssistant/", "/build/", "/dist/", "/.venv/",
        "/venv/", "/__pycache__/", "/.dart_tool/", "/.idea/", "/.vs/", "/target/",
    };
    const QString p = QDir::fromNativeSeparators(path);
    for (const QString& s : skipped) {
        if (p.contains(s))
            return true;
    }
    return false;
}

constexpr int kBatch = 16; // texts per embedding request

} // namespace

Settings Settings::current()
{
    AppConfig cfg;
    cfg.load();
    QSettings s;
    Settings out;
    out.provider = cfg.provider;
    out.endpoint = cfg.endpoint.isValid() && !cfg.endpoint.isEmpty()
            ? cfg.endpoint
            : LlmClient::defaultEndpoint(LlmClient::providerFromString(cfg.provider));
    out.apiKey = s.value(QStringLiteral("model/apiKey")).toString();
    out.chatModel = cfg.selectedModel;
    out.embeddingModel = s.value(QStringLiteral("rag/embeddingModel"), QStringLiteral("nomic-embed-text")).toString().trimmed();
    out.chunkChars = s.value(QStringLiteral("rag/chunkChars"), 1200).toInt();
    out.overlapChars = s.value(QStringLiteral("rag/overlapChars"), 200).toInt();
    out.topK = s.value(QStringLiteral("rag/topK"), 6).toInt();
    return out;
}

Index::Index(QObject* parent)
    : QObject(parent)
{
}

Index::~Index()
{
    cancel();
    if (m_thread) {
        m_thread->wait();
    }
}

QString Index::databasePathFor(const QString& folder)
{
    return QDir(folder).filePath(QStringLiteral("DevelopersAssistant/rag.sqlite"));
}

bool Index::isBusy() const
{
    return m_thread && m_thread->isRunning();
}

void Index::cancel()
{
    m_cancel = true;
}

void Index::runInThread(std::function<void()> work)
{
    m_cancel = false;
    QThread* thread = QThread::create(std::move(work));
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    m_thread = thread;
    thread->start();
}

void Index::startIndexing(const QString& folder)
{
    if (isBusy() || folder.isEmpty())
        return;
    const Settings settings = Settings::current();
    QPointer<Index> self(this);
    const auto post = [self](std::function<void(Index*)> f) {
        QMetaObject::invokeMethod(qApp, [self, f]() { if (self) f(self); }, Qt::QueuedConnection);
    };

    runInThread([this, folder, settings, post]() {
        // Which files to index.
        QStringList files;
        QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (!isSkippedPath(path) && isIndexable(it.fileInfo()))
                files << QDir::cleanPath(path);
        }

        Store store(databasePathFor(folder));
        if (!store.isOpen()) {
            const QString error = store.lastError();
            post([error](Index* i) { emit i->failed(QCoreApplication::translate("Rag", "Cannot open the index: %1").arg(error)); });
            return;
        }
        const Embedder embedder(settings.endpoint, Embedder::apiForProvider(settings.provider),
                                settings.embeddingModel, settings.apiKey);

        int indexed = 0, skipped = 0, unchanged = 0, chunksTotal = 0;
        QStringList problems;
        for (int f = 0; f < files.size(); ++f) {
            if (m_cancel)
                break;
            const QString& path = files.at(f);
            const QFileInfo fi(path);
            post([f, n = files.size(), name = fi.fileName()](Index* i) { emit i->progress(f, n, name); });

            const qint64 modified = fi.lastModified().toMSecsSinceEpoch();
            if (store.isUpToDate(path, modified, fi.size(), embedder.model())) {
                ++unchanged;
                continue;
            }
            QString error;
            const QString text = extractText(path, &error);
            const QStringList pieces = chunkText(text, settings.chunkChars, settings.overlapChars);
            if (pieces.isEmpty()) {
                ++skipped;
                if (!error.isEmpty() && problems.size() < 5)
                    problems << error;
                continue;
            }

            QVector<Chunk> chunks;
            bool ok = true;
            for (int start = 0; start < pieces.size() && ok; start += kBatch) {
                const QStringList batch = pieces.mid(start, kBatch);
                const QVector<QVector<float>> vectors = embedder.embed(batch, &error);
                if (vectors.isEmpty()) {
                    ok = false;
                    break;
                }
                for (int b = 0; b < batch.size(); ++b)
                    chunks.push_back(Chunk{path, start + b, batch.at(b), vectors.at(b)});
            }
            if (!ok) {
                // A server problem stops the whole run: every other file would fail the same way.
                post([error](Index* i) { emit i->failed(error); });
                return;
            }
            if (!store.replaceDocument(path, modified, fi.size(), embedder.model(), chunks)) {
                const QString dbError = store.lastError();
                post([dbError](Index* i) { emit i->failed(QCoreApplication::translate("Rag", "Cannot write the index: %1").arg(dbError)); });
                return;
            }
            ++indexed;
            chunksTotal += chunks.size();
        }

        const int removed = m_cancel ? 0 : store.removeMissing(QSet<QString>(files.begin(), files.end()));
        QString summary = m_cancel
                ? QCoreApplication::translate("Rag", "Stopped. Indexed %1 files (%2 chunks).").arg(indexed).arg(chunksTotal)
                : QCoreApplication::translate("Rag", "Indexed %1 files (%2 chunks), %3 unchanged, %4 skipped, %5 removed. "
                      "The index has %6 files and %7 chunks.")
                          .arg(indexed).arg(chunksTotal).arg(unchanged).arg(skipped).arg(removed)
                          .arg(store.documentCount()).arg(store.chunkCount());
        if (!problems.isEmpty())
            summary += QStringLiteral("\n") + problems.join(QStringLiteral("\n"));
        post([summary, n = files.size()](Index* i) {
            emit i->progress(n, n, QString());
            emit i->indexingFinished(summary);
        });
    });
}

QVector<Hit> Index::search(const QString& folder, const QString& query, int k, QString* error)
{
    const QString dbPath = databasePathFor(folder);
    if (folder.isEmpty() || !QFileInfo::exists(dbPath)) {
        if (error)
            *error = QCoreApplication::translate("Rag", "These documents are not indexed yet. Open Settings > Documents and index the project.");
        return {};
    }
    const Settings settings = Settings::current();
    const Embedder embedder(settings.endpoint, Embedder::apiForProvider(settings.provider),
                            settings.embeddingModel, settings.apiKey);
    const QVector<QVector<float>> q = embedder.embed({query}, error);
    if (q.isEmpty())
        return {};
    Store store(dbPath);
    if (!store.isOpen()) {
        if (error) *error = store.lastError();
        return {};
    }
    return store.search(q.first(), k > 0 ? k : settings.topK, embedder.model());
}

QString Index::buildAnswerPrompt(const QString& question, const QVector<Hit>& hits)
{
    QString prompt = QStringLiteral("Document excerpts:\n\n");
    for (int i = 0; i < hits.size(); ++i) {
        const Hit& h = hits.at(i);
        prompt += QStringLiteral("[%1] %2 (part %3)\n%4\n\n")
                .arg(i + 1).arg(QFileInfo(h.chunk.path).fileName()).arg(h.chunk.ordinal + 1).arg(h.chunk.text);
    }
    prompt += QStringLiteral("Question: ") + question;
    return prompt;
}

void Index::startAsking(const QString& folder, const QString& question)
{
    if (isBusy() || question.trimmed().isEmpty())
        return;
    QPointer<Index> self(this);
    const auto post = [self](std::function<void(Index*)> f) {
        QMetaObject::invokeMethod(qApp, [self, f]() { if (self) f(self); }, Qt::QueuedConnection);
    };
    runInThread([folder, question, post]() {
        const Settings settings = Settings::current();
        QString error;
        const QVector<Hit> hits = search(folder, question, settings.topK, &error);
        if (hits.isEmpty()) {
            post([error](Index* i) {
                emit i->failed(error.isEmpty() ? QCoreApplication::translate("Rag", "Nothing relevant was found in the index.") : error);
            });
            return;
        }
        if (settings.chatModel.isEmpty()) {
            post([](Index* i) { emit i->failed(QCoreApplication::translate("Rag", "Select a chat model first.")); });
            return;
        }

        // This is a worker thread: the client lives here and waiting for the answer is fine.
        LlmClient model;
        model.setProvider(LlmClient::providerFromString(settings.provider));
        model.setEndpoint(settings.endpoint);
        model.setApiKey(settings.apiKey);
        model.configure(settings.chatModel, {{QStringLiteral("temperature"), 0.1}});
        ModelRequest request;
        request.system = QStringLiteral(
                "You answer questions about the user's documents using only the excerpts given. "
                "Cite the excerpts you use as [1], [2] and so on. If the excerpts do not contain "
                "the answer, say so plainly. Answer in the language of the question.");
        request.user = buildAnswerPrompt(question, hits);
        request.timeoutMs = 300000;
        ModelReply* reply = model.send(request);
        reply->waitForFinished();
        const QString answer = reply->text();
        const QString answerError = reply->errorString();
        post([answer, answerError, hits](Index* i) {
            if (!answerError.isEmpty())
                emit i->failed(answerError);
            else
                emit i->answerReady(answer, hits);
        });
    });
}

} // namespace Rag
