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
    return generateWithSystemAndImages(userPrompt, systemPrompt, QStringList(), options);
}

QString OllamaAdapter::generateWithSystemAndImages(const QString& userPrompt,
                                                   const QString& systemPrompt,
                                                   const QStringList& imagesBase64,
                                                   const QVariantMap& options)
{
    QUrl url = m_endpoint;
    url.setPath("/api/chat");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QVariantMap requestOptions = options;
    const int timeoutMs = requestOptions.contains("timeoutMs") ? requestOptions.value("timeoutMs").toInt() : 30000;
    requestOptions.remove("timeoutMs");

    QJsonObject body = buildRequestBody(userPrompt, requestOptions, false, systemPrompt);

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
    if (!imagesBase64.isEmpty()) {
        QJsonArray imgs;
        for (const QString& b64 : imagesBase64) {
            if (!b64.trimmed().isEmpty())
                imgs.append(b64);
        }
        if (!imgs.isEmpty())
            userMsg["images"] = imgs;
    }
    messages.append(userMsg);
    body.insert("messages", messages);

    QEventLoop loop;
    QNetworkReply* r = m_nam.post(req, QJsonDocument(body).toJson());
    QObject::connect(r, SIGNAL(finished()), &loop, SLOT(quit()));
    QTimer t;
    t.setSingleShot(true);
    QObject::connect(&t, SIGNAL(timeout()), &loop, SLOT(quit()));
    t.start(qMax(1000, timeoutMs));
    loop.exec();

    if (r->error()!=QNetworkReply::NoError) {
        QString err = r->errorString();
        r->deleteLater();
        return QString("[Ollama error] %1").arg(err);
    }
    QByteArray data = r->readAll();
    r->deleteLater();
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
    startStreamWithSystem(prompt, QString(), options);
}

void OllamaAdapter::startStreamWithSystem(const QString& userPrompt,
                                          const QString& systemPrompt,
                                          const QVariantMap& options) {
    startStreamWithSystemAndImages(userPrompt, systemPrompt, QStringList(), options);
}

void OllamaAdapter::startStreamWithSystemAndImages(const QString& userPrompt,
                                                   const QString& systemPrompt,
                                                   const QStringList& imagesBase64,
                                                   const QVariantMap& options)
{
    if (m_streamReply) { m_streamReply->abort(); m_streamReply->deleteLater(); }
    m_buffer.clear();

    QUrl url = m_endpoint; url.setPath("/api/chat");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QVariantMap requestOptions = options;
    requestOptions.remove("timeoutMs");

    QJsonObject body = buildRequestBody(userPrompt, requestOptions, true, systemPrompt);
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
    if (!imagesBase64.isEmpty()) {
        QJsonArray imgs;
        for (const QString& b64 : imagesBase64) {
            if (!b64.trimmed().isEmpty())
                imgs.append(b64);
        }
        if (!imgs.isEmpty())
            userMsg["images"] = imgs;
    }
    messages.append(userMsg);
    body.insert("messages", messages);
    m_lastBody = body;

    m_streamReply = m_nam.post(req, QJsonDocument(body).toJson());
    QObject::connect(m_streamReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    QObject::connect(m_streamReply, SIGNAL(finished()), this, SLOT(onStreamFinished()));
}

void OllamaAdapter::stopStream(){
    if (m_streamReply) { m_streamReply->abort(); m_streamReply->deleteLater(); m_streamReply=nullptr; }
}

void OllamaAdapter::onReadyRead(){
    m_buffer += m_streamReply->readAll();
    while (true) {
        int nl = m_buffer.indexOf('\n');
        if (nl<0) break;
        QByteArray one = m_buffer.left(nl);
        m_buffer.remove(0,nl+1);
        if (one.trimmed().isEmpty()) continue; // Skip empty lines
        QJsonObject o = QJsonDocument::fromJson(one).object();
        QString part = o.value("message").toObject().value("content").toString();
        if (!part.isEmpty()) emit streamChunk(part);
    }
}

void OllamaAdapter::onStreamFinished(){
    emit streamFinished();
    if (m_streamReply) { m_streamReply->deleteLater(); m_streamReply=nullptr; }
}

QJsonObject OllamaAdapter::buildRequestBody(const QString& prompt,
                                            const QVariantMap& options,
                                            bool stream,
                                            const QString& systemPrompt) const
{
    QJsonObject body;
    body.insert("model", m_model);
    body.insert("stream", stream);

    // Create options object for generation parameters
    QJsonObject optionsObj;

    // Add only standard Ollama parameters to options object
    QJsonObject genParamsObj = m_genParams.toJsonObject();
    for (auto it = genParamsObj.begin(); it != genParamsObj.end(); ++it) {
        QString key = it.key();
        // Filter out parameters that cause warnings in Ollama
        if (key != "reasoning_effort" && key != "temperature_decay" && 
            key != "mirostat" && key != "mirostat_eta" && key != "mirostat_tau") {
            optionsObj.insert(key, it.value());
        }
    }

    // Add parameters from m_params (from configure) to options object
    for (auto it = m_params.begin(); it != m_params.end(); ++it) {
        QString key = it.key(); // it.key() already returns QString in QVariantMap
        // Filter out parameters that cause warnings in Ollama
        if (key != "reasoning_effort" && key != "temperature_decay" && 
            key != "mirostat" && key != "mirostat_eta" && key != "mirostat_tau") {
            optionsObj.insert(key, QJsonValue::fromVariant(it.value()));
        }
    }
    
    // Add parameters from options (local overrides) to options object
    for (auto it = options.begin(); it != options.end(); ++it) {
        QString key = it.key();
        // Filter out parameters that cause warnings in Ollama
        if (key != "reasoning_effort" && key != "temperature_decay" && 
            key != "mirostat" && key != "mirostat_eta" && key != "mirostat_tau") {
            optionsObj.insert(key, QJsonValue::fromVariant(it.value()));
        }
    }

    // Insert options object if it has values
    if (!optionsObj.isEmpty()) {
        body.insert("options", optionsObj);
    }

    return body;
}
