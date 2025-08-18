#include "OllamaAdapter.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>

OllamaAdapter::OllamaAdapter(QObject* parent):QObject(parent){ m_endpoint = QUrl("http://127.0.0.1:11434"); }

void OllamaAdapter::setEndpoint(const QUrl& url){ m_endpoint = url; }

void OllamaAdapter::configure(const QString& model, const QVariantMap& options){
    m_model = model;
    m_params = options;
}

QString OllamaAdapter::generate(const QString& prompt, const QVariantMap& options){
    QUrl url = m_endpoint; url.setPath("/api/generate");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body.insert("model", m_model);
    body.insert("prompt", prompt);
    body.insert("stream", false);

    for (auto it = m_params.begin(); it!=m_params.end(); ++it) body.insert(it.key(), QJsonValue::fromVariant(it.value()));
    for (auto it = options.begin(); it!=options.end(); ++it) body.insert(it.key(), QJsonValue::fromVariant(it.value()));

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
    return json.value("response").toString();
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
    if (m_streamReply) { m_streamReply->abort(); m_streamReply->deleteLater(); }
    m_buffer.clear();

    QUrl url = m_endpoint; url.setPath("/api/generate");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body.insert("model", m_model);
    body.insert("prompt", prompt);
    body.insert("stream", true);
    for (auto it = m_params.begin(); it!=m_params.end(); ++it) body.insert(it.key(), QJsonValue::fromVariant(it.value()));
    for (auto it = options.begin(); it!=options.end(); ++it) body.insert(it.key(), QJsonValue::fromVariant(it.value()));
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
        QJsonObject o = QJsonDocument::fromJson(one).object();
        QString part = o.value("response").toString();
        if (!part.isEmpty()) emit streamChunk(part);
    }
}

void OllamaAdapter::onStreamFinished(){
    emit streamFinished();
    if (m_streamReply) { m_streamReply->deleteLater(); m_streamReply=nullptr; }
}
