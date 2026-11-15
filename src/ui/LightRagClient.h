#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QStringList>
#include <QUrl>

#include <functional>

class QNetworkReply;
class QUrlQuery;

// Client for a LightRAG server (github.com/HKUDS/LightRAG, `lightrag-server`, port 9621 by
// default). The server has its own LLM and embedding model; this client uploads documents,
// asks questions in one of LightRAG's retrieval modes and reads the knowledge graph.
//
// All calls are asynchronous; each result or error arrives as a signal.
class LightRagClient : public QObject
{
    Q_OBJECT

public:
    explicit LightRagClient(QObject* parent = nullptr);

    // Server address, e.g. http://localhost:9621, and the optional API key
    // (LIGHTRAG_API_KEY on the server, sent as X-API-Key).
    void setServer(const QUrl& baseUrl, const QString& apiKey = QString());
    QUrl server() const { return m_base; }

    void checkHealth();                                   // GET  /health
    void uploadFile(const QString& filePath);             // POST /documents/upload
    void listDocuments();                                 // GET  /documents
    void query(const QString& question, const QString& mode); // POST /query
    void listGraphLabels();                               // GET  /graph/label/list
    void loadGraph(const QString& label, int maxDepth = 2, int maxNodes = 300); // GET /graphs

signals:
    void healthChecked(bool ok, const QString& details);
    void fileUploaded(const QString& filePath, bool ok, const QString& message);
    void documentsListed(const QJsonArray& documents);   // one object per document: id, file_path, status, ...
    void answered(const QString& answer);
    void graphLabelsListed(const QStringList& labels);
    void graphLoaded(const QJsonArray& nodes, const QJsonArray& edges);
    void failed(const QString& what, const QString& error);

private:
    QNetworkRequest request(const QString& path, const QUrlQuery* query = nullptr) const;
    // Calls `onSuccess` with the JSON body, or emits failed(what, reason).
    void handle(QNetworkReply* reply, const QString& what, std::function<void(const QJsonDocument&)> onSuccess);

    QNetworkAccessManager m_nam;
    QUrl m_base = QUrl(QStringLiteral("http://localhost:9621"));
    QString m_apiKey;
};
