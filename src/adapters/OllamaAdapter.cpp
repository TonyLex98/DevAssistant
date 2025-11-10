#include "OllamaAdapter.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>

OllamaAdapter::OllamaAdapter(QObject* parent):QObject(parent){
    m_endpoint = QUrl("http://127.0.0.1:11434");
    // Инициализируем параметры генерации значениями по умолчанию
    // Особенно важно для генерации JSON-ответов
}

void OllamaAdapter::setEndpoint(const QUrl& url){ m_endpoint = url; }

void OllamaAdapter::configure(const QString& model, const QVariantMap& options){
    m_model = model;
    m_params = options;
}

QString OllamaAdapter::generate(const QString& prompt, const QVariantMap& options){
    return generateWithSystem(prompt, QString(), options);
}

QString OllamaAdapter::generateWithSystem(const QString& userPrompt,
                                          const QString& systemPrompt,
                                          const QVariantMap& options){
    QUrl url = m_endpoint; url.setPath("/api/chat");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body = buildRequestBody(userPrompt, options, false, systemPrompt);

    QJsonArray messages;
    if (!systemPrompt.isEmpty()) {
        QJsonObject systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = systemPrompt;
        messages.append(systemMsg);
    }
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userPrompt;
    messages.append(userMsg);
    body.insert("messages", messages);

    QEventLoop loop;
    QNetworkReply* r = m_nam.post(req, QJsonDocument(body).toJson());
    QObject::connect(r, SIGNAL(finished()), &loop, SLOT(quit()));
    QTimer t; t.setSingleShot(true); QObject::connect(&t, SIGNAL(timeout()), &loop, SLOT(quit())); t.start(30000);
    loop.exec();

    if (r->error()!=QNetworkReply::NoError) {
        QString err = r->errorString(); r->deleteLater(); return QString("[Ollama error] %1").arg(err);
    }
    QByteArray data = r->readAll(); r->deleteLater();
    QJsonObject json = QJsonDocument::fromJson(data).object();
    return json.value("message").toObject().value("content").toString();
}

int OllamaAdapter::estimateTokens(const QString& text) const {
    return text.size()/4 + 1;
}

bool OllamaAdapter::isAvailable(QString* errorOut){
    QUrl url = m_endpoint; url.setPath("/api/tags");
    QNetworkRequest req(url);
    QEventLoop loop;
    QNetworkReply* r = m_nam.get(req);
    QObject::connect(r, SIGNAL(finished()), &loop, SLOT(quit()));
    QTimer t; t.setSingleShot(true); QObject::connect(&t, SIGNAL(timeout()), &loop, SLOT(quit())); t.start(8000);
    loop.exec();
    if (r->error()!=QNetworkReply::NoError) {
        if (errorOut) *errorOut = r->errorString();
        r->deleteLater(); return false;
    }
    r->deleteLater(); return true;
}

QStringList OllamaAdapter::listModels(){
    QStringList out;
    QUrl url = m_endpoint; url.setPath("/api/tags");
    QNetworkRequest req(url);
    QEventLoop loop;
    QNetworkReply* r = m_nam.get(req);
    QObject::connect(r, SIGNAL(finished()), &loop, SLOT(quit()));
    QTimer t; t.setSingleShot(true); QObject::connect(&t, SIGNAL(timeout()), &loop, SLOT(quit())); t.start(8000);
    loop.exec();
    if (r->error()!=QNetworkReply::NoError) { r->deleteLater(); return out; }
    QByteArray data = r->readAll();
    r->deleteLater();
    QJsonObject o = QJsonDocument::fromJson(data).object();
    QJsonArray models = o.value("models").toArray();
    for (auto v: models) {
        QJsonObject m = v.toObject();
        out << m.value("name").toString();
    }
    return out;
}

void OllamaAdapter::startStream(const QString& prompt, const QVariantMap& options) {
    QUrl url = m_endpoint; url.setPath("/api/chat");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body = buildRequestBody(prompt, options, true, QString());

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = prompt;
    messages.append(userMsg);
    body.insert("messages", messages);

    QNetworkReply* r = m_nam.post(req, QJsonDocument(body).toJson());
    m_streamReply = r;
    QObject::connect(r, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    QObject::connect(r, SIGNAL(finished()), this, SLOT(onStreamFinished()));
}

void OllamaAdapter::startStreamWithSystem(const QString& userPrompt,
                                          const QString& systemPrompt,
                                          const QVariantMap& options) {
    QUrl url = m_endpoint; url.setPath("/api/chat");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body = buildRequestBody(userPrompt, options, true, systemPrompt);

    QJsonArray messages;
    if (!systemPrompt.isEmpty()) {
        QJsonObject systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = systemPrompt;
        messages.append(systemMsg);
    }
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userPrompt;
    messages.append(userMsg);
    body.insert("messages", messages);

    QNetworkReply* r = m_nam.post(req, QJsonDocument(body).toJson());
    m_streamReply = r;
    QObject::connect(r, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    QObject::connect(r, SIGNAL(finished()), this, SLOT(onStreamFinished()));
}

void OllamaAdapter::stopStream() {
    if (m_streamReply) {
        m_streamReply->abort();
        m_streamReply = nullptr;
    }
}

void OllamaAdapter::onReadyRead() {
    if (!m_streamReply) return;
    m_buffer += m_streamReply->readAll();
    int lastNewLine = -1;
    forever {
        int nextNewLine = m_buffer.indexOf('\n', lastNewLine+1);
        if (nextNewLine == -1) break;
        QByteArray line = m_buffer.mid(lastNewLine+1, nextNewLine - lastNewLine - 1);
        lastNewLine = nextNewLine;
        if (line.trimmed().isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) continue;
        QJsonObject obj = doc.object();
        if (obj.contains("error")) {
            emit streamError(obj.value("error").toString());
            return; // stop processing
        }
        if (obj.contains("message")) {
            QJsonObject msg = obj.value("message").toObject();
            QString content = msg.value("content").toString();
            if (!content.isEmpty())
                emit streamChunk(content);
        }
        if (obj.value("done").toBool()) {
            emit streamFinished();
        }
    }
}

void OllamaAdapter::onStreamFinished() {
    if (!m_streamReply) return;
    onReadyRead(); // process remainder
    m_streamReply->deleteLater();
    m_streamReply = nullptr;
}

QJsonObject OllamaAdapter::buildRequestBody(const QString& prompt,
                                            const QVariantMap& options,
                                            bool stream,
                                            const QString& systemPrompt) const {
    QJsonObject o;
    o["model"] = m_model;
    o["stream"] = stream;
    if (!systemPrompt.isEmpty()) {
        o["options"] = m_genParams.toJsonObject();
    } else {
        // Apply options from the parameter map
        if (options.contains("temperature")) o["temperature"] = options.value("temperature").toDouble();
        if (options.contains("top_p")) o["top_p"] = options.value("top_p").toDouble();
        if (options.contains("top_k")) o["top_k"] = options.value("top_k").toInt();
        if (options.contains("repeat_penalty")) o["repeat_penalty"] = options.value("repeat_penalty").toDouble();
        if (options.contains("presence_penalty")) o["presence_penalty"] = options.value("presence_penalty").toDouble();
        if (options.contains("frequency_penalty")) o["frequency_penalty"] = options.value("frequency_penalty").toDouble();
        if (options.contains("num_predict")) o["num_predict"] = options.value("num_predict").toInt();
        if (options.contains("num_ctx")) o["num_ctx"] = options.value("num_ctx").toInt();
        if (options.contains("mirostat")) o["mirostat"] = options.value("mirostat").toInt();
        if (options.contains("mirostat_tau")) o["mirostat_tau"] = options.value("mirostat_tau").toDouble();
        if (options.contains("mirostat_eta")) o["mirostat_eta"] = options.value("mirostat_eta").toDouble();
        if (options.contains("seed")) o["seed"] = qint64(options.value("seed").toULongLong());
        if (options.contains("stop")) {
            QJsonArray stopArray = options.value("stop").toJsonArray();
            o["stop"] = stopArray;
        }
        if (options.contains("reasoning_effort")) o["reasoning_effort"] = options.value("reasoning_effort").toString();
        if (options.contains("temperature_decay")) o["temperature_decay"] = options.value("temperature_decay").toDouble();
        if (options.contains("repeat_last_n")) o["repeat_last_n"] = options.value("repeat_last_n").toInt();
    }
    return o;
}

// Method to get embeddings using a specific model
QVector<float> OllamaAdapter::getEmbedding(const QString& text, const QString& model) {
    QUrl url = m_endpoint; 
    url.setPath("/api/embeddings");
    
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject payload;
    payload["model"] = model.isEmpty() ? "nomic-embed-text" : model;
    payload["input"] = text;
    
    QEventLoop loop;
    QNetworkReply* r = m_nam.post(req, QJsonDocument(payload).toJson());
    QObject::connect(r, SIGNAL(finished()), &loop, SLOT(quit()));
    QTimer t; 
    t.setSingleShot(true); 
    QObject::connect(&t, SIGNAL(timeout()), &loop, SLOT(quit())); 
    t.start(30000);  // 30 second timeout
    loop.exec();

    if (r->error() != QNetworkReply::NoError) {
        QString err = r->errorString();
        r->deleteLater(); 
        qWarning() << "Embedding error:" << err;
        return QVector<float>(); // Return empty vector
    }
    
    QByteArray data = r->readAll(); 
    r->deleteLater();
    
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    QJsonObject json = jsonDoc.object();
    
    // Ollama v2+ returns embeddings in an array
    QJsonArray embeddingsArray = json["embeddings"].toArray();
    if (!embeddingsArray.isEmpty()) {
        QJsonArray firstEmbedding = embeddingsArray[0].toArray();
        QVector<float> result;
        for (const QJsonValue& val : firstEmbedding) {
            result.append(static_cast<float>(val.toDouble()));
        }
        return result;
    }
    
    // Fallback: older format or different structure
    // Some models might return embeddings as a direct array rather than in an embeddings object
    QJsonArray arr = json["embedding"].toArray(); // Different possible field name
    if (!arr.isEmpty()) {
        QVector<float> result;
        for (const QJsonValue& val : arr) {
            result.append(static_cast<float>(val.toDouble()));
        }
        return result;
    }
    
    qWarning() << "Could not parse embedding response:" << data;
    return QVector<float>(); // Return empty vector
}