#include "core/model/OllamaAdapter.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>

namespace {
QJsonValue stopValueToJson(const QVariant& stopVariant)
{
    if (!stopVariant.isValid() || stopVariant.isNull())
        return QJsonValue();

    if (stopVariant.type() == QVariant::String)
        return QJsonValue(stopVariant.toString());

    if (stopVariant.canConvert<QJsonArray>())
        return QJsonValue(stopVariant.toJsonArray());

    if (stopVariant.type() == QVariant::StringList) {
        QJsonArray arr;
        const QStringList values = stopVariant.toStringList();
        for (const QString& value : values) {
            const QString trimmed = value.trimmed();
            if (!trimmed.isEmpty())
                arr.append(trimmed);
        }
        return arr;
    }

    if (stopVariant.type() == QVariant::List) {
        QJsonArray arr;
        const QVariantList values = stopVariant.toList();
        for (const QVariant& value : values) {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty())
                arr.append(text);
        }
        return arr;
    }

    return QJsonValue();
}

QString extractOpenAIMessageContent(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();

    if (value.isArray()) {
        QStringList parts;
        const QJsonArray array = value.toArray();
        for (const QJsonValue& item : array) {
            if (item.isString()) {
                const QString part = item.toString().trimmed();
                if (!part.isEmpty())
                    parts << part;
                continue;
            }
            if (!item.isObject())
                continue;

            const QJsonObject obj = item.toObject();
            const QString type = obj.value("type").toString().trimmed().toLower();
            QString text;
            if (type == "text")
                text = obj.value("text").toString();
            else
                text = obj.value("content").toString();

            text = text.trimmed();
            if (!text.isEmpty())
                parts << text;
        }
        return parts.join("\n");
    }

    if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        QString text = obj.value("text").toString().trimmed();
        if (text.isEmpty())
            text = obj.value("content").toString().trimmed();
        return text;
    }

    return QString();
}
} // namespace

OllamaAdapter::OllamaAdapter(QObject* parent):QObject(parent)
{
    m_provider = Provider::Ollama;
    m_endpoint = defaultEndpointForProvider(m_provider);
}

void OllamaAdapter::setEndpoint(const QUrl& url)
{
    m_endpoint = url;
}

void OllamaAdapter::setProvider(Provider provider)
{
    m_provider = provider;
}

void OllamaAdapter::setProvider(const QString& providerName)
{
    m_provider = providerFromString(providerName);
}

QString OllamaAdapter::providerName() const
{
    return providerToString(m_provider);
}

OllamaAdapter::Provider OllamaAdapter::providerFromString(const QString& providerName)
{
    const QString normalized = providerName.trimmed().toLower();
    if (normalized == "llama.cpp" || normalized == "llamacpp" || normalized == "llama-cpp" || normalized == "llama")
        return Provider::LlamaCpp;
    return Provider::Ollama;
}

QString OllamaAdapter::providerToString(Provider provider)
{
    return (provider == Provider::LlamaCpp) ? QStringLiteral("llama.cpp")
                                            : QStringLiteral("ollama");
}

QUrl OllamaAdapter::defaultEndpointForProvider(Provider provider)
{
    return (provider == Provider::LlamaCpp)
               ? QUrl(QStringLiteral("http://127.0.0.1:8080"))
               : QUrl(QStringLiteral("http://127.0.0.1:11434"));
}

void OllamaAdapter::configure(const QString& model, const QVariantMap& options)
{
    m_model = model;
    m_params = options;
}

QString OllamaAdapter::generate(const QString& prompt, const QVariantMap& options)
{
    return generateWithSystem(prompt, QString(), options);
}

QString OllamaAdapter::generateWithSystem(const QString& userPrompt,
                                          const QString& systemPrompt,
                                          const QVariantMap& options)
{
    return generateWithSystemAndImages(userPrompt, systemPrompt, QStringList(), options);
}

QString OllamaAdapter::generateWithSystemAndImages(const QString& userPrompt,
                                                   const QString& systemPrompt,
                                                   const QStringList& imagesBase64,
                                                   const QVariantMap& options)
{
    QUrl url = m_endpoint;
    url.setPath(m_provider == Provider::LlamaCpp ? "/v1/chat/completions" : "/api/chat");

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QVariantMap requestOptions = options;
    const int timeoutMs = requestOptions.contains("timeoutMs") ? requestOptions.value("timeoutMs").toInt() : 30000;
    requestOptions.remove("timeoutMs");

    QJsonObject body;
    if (m_provider == Provider::LlamaCpp) {
        body = buildLlamaCppRequestBody(userPrompt, systemPrompt, imagesBase64, requestOptions, false);
    } else {
        body = buildRequestBody(userPrompt, requestOptions, false, systemPrompt);

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
    }

    QEventLoop loop;
    QNetworkReply* r = m_nam.post(req, QJsonDocument(body).toJson());
    QObject::connect(r, SIGNAL(finished()), &loop, SLOT(quit()));

    QTimer t;
    t.setSingleShot(true);
    QObject::connect(&t, SIGNAL(timeout()), &loop, SLOT(quit()));
    t.start(qMax(1000, timeoutMs));

    loop.exec();

    // A timeout leaves the request running: stop it and report it, instead of returning
    // whatever partial text has arrived as if it were the answer.
    if (!r->isFinished()) {
        r->abort();
        r->deleteLater();
        return QString("[Ollama error] %1").arg(QCoreApplication::translate("OllamaAdapter",
                "no answer within %1 s").arg(qMax(1000, timeoutMs) / 1000));
    }

    if (r->error() != QNetworkReply::NoError) {
        const QString err = r->errorString();
        r->deleteLater();
        return QString("[Ollama error] %1").arg(err);
    }

    const QByteArray data = r->readAll();
    r->deleteLater();

    const QJsonObject json = QJsonDocument::fromJson(data).object();
    if (m_provider == Provider::LlamaCpp) {
        const QJsonObject error = json.value("error").toObject();
        if (!error.isEmpty()) {
            const QString errorText = error.value("message").toString().trimmed();
            if (!errorText.isEmpty())
                return QString("[Ollama error] %1").arg(errorText);
        }

        const QJsonArray choices = json.value("choices").toArray();
        if (!choices.isEmpty()) {
            const QJsonObject message = choices.first().toObject().value("message").toObject();
            const QString content = extractOpenAIMessageContent(message.value("content")).trimmed();
            if (!content.isEmpty())
                return content;
        }

        return QString();
    }

    return json.value("message").toObject().value("content").toString();
}

int OllamaAdapter::estimateTokens(const QString& text) const
{
    return text.size() / 4 + 1;
}

bool OllamaAdapter::isAvailable(QString* errorOut)
{
    QUrl url = m_endpoint;
    url.setPath(m_provider == Provider::LlamaCpp ? "/v1/models" : "/api/tags");

    QNetworkRequest req(url);
    QEventLoop loop;
    QNetworkReply* r = m_nam.get(req);
    QObject::connect(r, SIGNAL(finished()), &loop, SLOT(quit()));

    QTimer t;
    t.setSingleShot(true);
    QObject::connect(&t, SIGNAL(timeout()), &loop, SLOT(quit()));
    t.start(8000);

    loop.exec();

    if (!r->isFinished()) {
        r->abort();
        if (errorOut)
            *errorOut = QCoreApplication::translate("OllamaAdapter", "no answer from %1 within 8 s")
                    .arg(m_endpoint.toString());
        r->deleteLater();
        return false;
    }
    if (r->error() != QNetworkReply::NoError) {
        if (errorOut)
            *errorOut = r->errorString();
        r->deleteLater();
        return false;
    }

    r->deleteLater();
    return true;
}

QStringList OllamaAdapter::listModels()
{
    QStringList out;
    auto getJson = [this](const QString& path, QJsonObject* outJson) -> bool {
        QUrl url = m_endpoint;
        url.setPath(path);
        QNetworkRequest req(url);
        QEventLoop loop;
        QNetworkReply* r = m_nam.get(req);
        QObject::connect(r, SIGNAL(finished()), &loop, SLOT(quit()));

        QTimer t;
        t.setSingleShot(true);
        QObject::connect(&t, SIGNAL(timeout()), &loop, SLOT(quit()));
        t.start(8000);
        loop.exec();

        if (!r->isFinished()) {
            r->abort();
            r->deleteLater();
            return false;
        }
        if (r->error() != QNetworkReply::NoError) {
            r->deleteLater();
            return false;
        }

        const QByteArray data = r->readAll();
        r->deleteLater();
        *outJson = QJsonDocument::fromJson(data).object();
        return true;
    };

    if (m_provider == Provider::LlamaCpp) {
        QJsonObject modelList;
        if (getJson(QStringLiteral("/v1/models"), &modelList)) {
            QJsonArray models = modelList.value("data").toArray();
            if (models.isEmpty())
                models = modelList.value("models").toArray();

            for (const QJsonValue& v : models) {
                const QJsonObject m = v.toObject();
                QString id = m.value("id").toString().trimmed();
                if (id.isEmpty())
                    id = m.value("name").toString().trimmed();
                if (!id.isEmpty() && !out.contains(id))
                    out << id;
            }
        }

        if (out.isEmpty()) {
            QJsonObject props;
            if (getJson(QStringLiteral("/props"), &props)) {
                QString modelId = props.value("model").toString().trimmed();
                if (modelId.isEmpty())
                    modelId = props.value("model_alias").toString().trimmed();
                if (modelId.isEmpty()) {
                    const QString modelPath = props.value("model_path").toString().trimmed();
                    if (!modelPath.isEmpty())
                        modelId = QFileInfo(modelPath).fileName().trimmed();
                }
                if (!modelId.isEmpty())
                    out << modelId;
            }
        }

        return out;
    }

    QJsonObject o;
    if (!getJson(QStringLiteral("/api/tags"), &o))
        return out;

    const QJsonArray models = o.value("models").toArray();
    for (const QJsonValue& v : models) {
        const QJsonObject m = v.toObject();
        const QString name = m.value("name").toString().trimmed();
        if (!name.isEmpty())
            out << name;
    }
    return out;
}

void OllamaAdapter::startStream(const QString& prompt, const QVariantMap& options)
{
    startStreamWithSystem(prompt, QString(), options);
}

void OllamaAdapter::startStreamWithSystem(const QString& userPrompt,
                                          const QString& systemPrompt,
                                          const QVariantMap& options)
{
    startStreamWithSystemAndImages(userPrompt, systemPrompt, QStringList(), options);
}

void OllamaAdapter::startStreamWithSystemAndImages(const QString& userPrompt,
                                                   const QString& systemPrompt,
                                                   const QStringList& imagesBase64,
                                                   const QVariantMap& options)
{
    if (m_streamReply) {
        m_streamReply->abort();
        m_streamReply->deleteLater();
    }
    m_buffer.clear();

    QUrl url = m_endpoint;
    url.setPath(m_provider == Provider::LlamaCpp ? "/v1/chat/completions" : "/api/chat");

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QVariantMap requestOptions = options;
    requestOptions.remove("timeoutMs");

    QJsonObject body;
    if (m_provider == Provider::LlamaCpp) {
        body = buildLlamaCppRequestBody(userPrompt, systemPrompt, imagesBase64, requestOptions, true);
    } else {
        body = buildRequestBody(userPrompt, requestOptions, true, systemPrompt);

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
    }

    m_lastBody = body;

    m_streamReply = m_nam.post(req, QJsonDocument(body).toJson());
    QObject::connect(m_streamReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    QObject::connect(m_streamReply, SIGNAL(finished()), this, SLOT(onStreamFinished()));
}

void OllamaAdapter::stopStream()
{
    if (m_streamReply) {
        m_streamReply->abort();
        m_streamReply->deleteLater();
        m_streamReply = nullptr;
    }
}

void OllamaAdapter::onReadyRead()
{
    m_buffer += m_streamReply->readAll();

    if (m_provider == Provider::LlamaCpp) {
        while (true) {
            const int nl = m_buffer.indexOf('\n');
            if (nl < 0)
                break;

            QByteArray one = m_buffer.left(nl);
            m_buffer.remove(0, nl + 1);

            one = one.trimmed();
            if (one.isEmpty() || !one.startsWith("data:"))
                continue;

            QByteArray payload = one.mid(5).trimmed();
            if (payload == "[DONE]")
                continue;

            const QJsonObject o = QJsonDocument::fromJson(payload).object();
            const QJsonArray choices = o.value("choices").toArray();
            if (choices.isEmpty())
                continue;

            const QJsonObject delta = choices.first().toObject().value("delta").toObject();
            const QString part = extractOpenAIMessageContent(delta.value("content"));
            if (!part.isEmpty())
                emit streamChunk(part);
        }

        return;
    }

    while (true) {
        const int nl = m_buffer.indexOf('\n');
        if (nl < 0)
            break;

        const QByteArray one = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);

        if (one.trimmed().isEmpty())
            continue;

        const QJsonObject o = QJsonDocument::fromJson(one).object();
        const QString part = o.value("message").toObject().value("content").toString();
        if (!part.isEmpty())
            emit streamChunk(part);
    }
}

void OllamaAdapter::onStreamFinished()
{
    if (m_streamReply &&
        m_streamReply->error() != QNetworkReply::NoError &&
        m_streamReply->error() != QNetworkReply::OperationCanceledError) {
        emit streamError(m_streamReply->errorString());
    }

    emit streamFinished();

    if (m_streamReply) {
        m_streamReply->deleteLater();
        m_streamReply = nullptr;
    }
}

QJsonObject OllamaAdapter::buildRequestBody(const QString& prompt,
                                            const QVariantMap& options,
                                            bool stream,
                                            const QString& systemPrompt) const
{
    Q_UNUSED(prompt);
    Q_UNUSED(systemPrompt);

    QJsonObject body;
    body.insert("model", m_model);
    body.insert("stream", stream);

    QJsonObject optionsObj;

    QJsonObject genParamsObj = m_genParams.toJsonObject();
    for (auto it = genParamsObj.begin(); it != genParamsObj.end(); ++it) {
        QString key = it.key();
        if (key != "reasoning_effort" && key != "temperature_decay" &&
            key != "mirostat" && key != "mirostat_eta" && key != "mirostat_tau") {
            optionsObj.insert(key, it.value());
        }
    }

    for (auto it = m_params.begin(); it != m_params.end(); ++it) {
        QString key = it.key();
        if (key != "reasoning_effort" && key != "temperature_decay" &&
            key != "mirostat" && key != "mirostat_eta" && key != "mirostat_tau") {
            optionsObj.insert(key, QJsonValue::fromVariant(it.value()));
        }
    }

    for (auto it = options.begin(); it != options.end(); ++it) {
        QString key = it.key();
        if (key != "reasoning_effort" && key != "temperature_decay" &&
            key != "mirostat" && key != "mirostat_eta" && key != "mirostat_tau") {
            optionsObj.insert(key, QJsonValue::fromVariant(it.value()));
        }
    }

    if (!optionsObj.isEmpty())
        body.insert("options", optionsObj);

    return body;
}

QJsonArray OllamaAdapter::buildMessages(const QString& userPrompt,
                                        const QString& systemPrompt,
                                        const QStringList& imagesBase64) const
{
    QJsonArray messages;

    if (!systemPrompt.isEmpty()) {
        QJsonObject systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = systemPrompt;
        messages.append(systemMsg);
    }

    QJsonObject userMsg;
    userMsg["role"] = "user";

    if (imagesBase64.isEmpty()) {
        userMsg["content"] = userPrompt;
        messages.append(userMsg);
        return messages;
    }

    QJsonArray userContent;

    QJsonObject textPart;
    textPart["type"] = "text";
    textPart["text"] = userPrompt;
    userContent.append(textPart);

    for (const QString& b64 : imagesBase64) {
        const QString trimmed = b64.trimmed();
        if (trimmed.isEmpty())
            continue;

        QJsonObject imageUrl;
        imageUrl["url"] = QString("data:image/png;base64,%1").arg(trimmed);

        QJsonObject imagePart;
        imagePart["type"] = "image_url";
        imagePart["image_url"] = imageUrl;
        userContent.append(imagePart);
    }

    userMsg["content"] = userContent;
    messages.append(userMsg);
    return messages;
}

QJsonObject OllamaAdapter::buildLlamaCppRequestBody(const QString& userPrompt,
                                                    const QString& systemPrompt,
                                                    const QStringList& imagesBase64,
                                                    const QVariantMap& options,
                                                    bool stream) const
{
    QJsonObject body;
    body["model"] = m_model;
    body["stream"] = stream;
    body["messages"] = buildMessages(userPrompt, systemPrompt, imagesBase64);

    QVariantMap merged = m_params;
    for (auto it = options.begin(); it != options.end(); ++it)
        merged[it.key()] = it.value();

    if (!merged.contains("temperature"))
        merged["temperature"] = m_genParams.temperature;
    if (!merged.contains("top_p"))
        merged["top_p"] = m_genParams.top_p;
    if (!merged.contains("top_k"))
        merged["top_k"] = m_genParams.top_k;
    if (!merged.contains("presence_penalty"))
        merged["presence_penalty"] = m_genParams.presence_penalty;
    if (!merged.contains("frequency_penalty"))
        merged["frequency_penalty"] = m_genParams.frequency_penalty;
    if (!merged.contains("repeat_penalty"))
        merged["repeat_penalty"] = m_genParams.repeat_penalty;
    if (!merged.contains("repeat_last_n"))
        merged["repeat_last_n"] = m_genParams.repeat_last_n;
    if (!merged.contains("seed"))
        merged["seed"] = qulonglong(m_genParams.seed);
    if (!merged.contains("num_predict"))
        merged["num_predict"] = m_genParams.num_predict;
    if (!merged.contains("stop") && !m_genParams.stop.isEmpty())
        merged["stop"] = m_genParams.stop;

    if (merged.contains("temperature"))
        body["temperature"] = merged.value("temperature").toDouble();
    if (merged.contains("top_p"))
        body["top_p"] = merged.value("top_p").toDouble();
    if (merged.contains("top_k"))
        body["top_k"] = merged.value("top_k").toInt();
    if (merged.contains("presence_penalty"))
        body["presence_penalty"] = merged.value("presence_penalty").toDouble();
    if (merged.contains("frequency_penalty"))
        body["frequency_penalty"] = merged.value("frequency_penalty").toDouble();
    if (merged.contains("repeat_penalty"))
        body["repeat_penalty"] = merged.value("repeat_penalty").toDouble();
    if (merged.contains("repeat_last_n"))
        body["repeat_last_n"] = merged.value("repeat_last_n").toInt();

    const qlonglong seed = merged.value("seed").toLongLong();
    if (seed != 0)
        body["seed"] = seed;

    const int maxTokens = merged.value("num_predict", -1).toInt();
    if (maxTokens >= 0)
        body["max_tokens"] = maxTokens;

    if (merged.contains("stop")) {
        const QJsonValue stopJson = stopValueToJson(merged.value("stop"));
        if (!stopJson.isNull() && !stopJson.isUndefined())
            body["stop"] = stopJson;
    }

    return body;
}
