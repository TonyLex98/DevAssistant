#include "core/model/LlmClient.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <memory>

namespace {

// Generation options that Ollama does not take in "options".
bool isOllamaOption(const QString& key)
{
    static const QStringList notForOllama = {
        QStringLiteral("reasoning_effort"), QStringLiteral("temperature_decay"),
        QStringLiteral("mirostat"), QStringLiteral("mirostat_eta"), QStringLiteral("mirostat_tau"),
        QStringLiteral("timeoutMs")
    };
    return !notForOllama.contains(key);
}

QJsonValue stopValueToJson(const QVariant& stop)
{
    if (!stop.isValid() || stop.isNull())
        return QJsonValue();
    if (stop.type() == QVariant::String)
        return QJsonValue(stop.toString());
    QJsonArray arr;
    QStringList values;
    if (stop.canConvert<QJsonArray>() && stop.type() != QVariant::StringList && stop.type() != QVariant::List) {
        for (const QJsonValue& v : stop.toJsonArray())
            values << v.toString();
    } else {
        values = stop.toStringList();
    }
    for (const QString& v : values) {
        const QString t = v.trimmed();
        if (!t.isEmpty())
            arr.append(t);
    }
    return arr.isEmpty() ? QJsonValue() : QJsonValue(arr);
}

// OpenAI-compatible servers return content as a string or as a list of parts.
QString openAiContent(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (value.isObject()) {
        const QJsonObject o = value.toObject();
        const QString text = o.value(QStringLiteral("text")).toString();
        return text.isEmpty() ? o.value(QStringLiteral("content")).toString() : text;
    }
    QStringList parts;
    for (const QJsonValue& item : value.toArray()) {
        if (item.isString()) {
            parts << item.toString();
            continue;
        }
        const QJsonObject o = item.toObject();
        const QString type = o.value(QStringLiteral("type")).toString().toLower();
        const QString text = (type == QLatin1String("text")) ? o.value(QStringLiteral("text")).toString()
                                                             : o.value(QStringLiteral("content")).toString();
        if (!text.trimmed().isEmpty())
            parts << text;
    }
    return parts.join(QLatin1Char('\n'));
}

QString errorFromJson(const QJsonObject& json)
{
    const QJsonValue error = json.value(QStringLiteral("error"));
    if (error.isString())
        return error.toString();
    return error.toObject().value(QStringLiteral("message")).toString();
}

} // namespace

LlmClient::LlmClient(QObject* parent)
    : ModelClient(parent)
    , m_endpoint(defaultEndpoint(Provider::Ollama))
{
}

LlmClient::Provider LlmClient::providerFromString(const QString& name)
{
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("llama.cpp") || n == QLatin1String("llamacpp") || n == QLatin1String("llama-cpp")
        || n == QLatin1String("llama") || n == QLatin1String("openai"))
        return Provider::OpenAiCompatible;
    return Provider::Ollama;
}

QString LlmClient::providerToString(Provider provider)
{
    return provider == Provider::OpenAiCompatible ? QStringLiteral("llama.cpp") : QStringLiteral("ollama");
}

QUrl LlmClient::defaultEndpoint(Provider provider)
{
    return provider == Provider::OpenAiCompatible ? QUrl(QStringLiteral("http://127.0.0.1:8080"))
                                                  : QUrl(QStringLiteral("http://127.0.0.1:11434"));
}

void LlmClient::configure(const QString& model, const QVariantMap& options)
{
    m_model = model;
    m_options = options;
}

void LlmClient::copySettingsFrom(const LlmClient& other)
{
    m_provider = other.m_provider;
    m_endpoint = other.m_endpoint;
    m_apiKey = other.m_apiKey;
    m_model = other.m_model;
    m_options = other.m_options;
}

QNetworkRequest LlmClient::httpRequest(const QString& path) const
{
    QUrl url = m_endpoint;
    url.setPath(path);
    QNetworkRequest req(url);
    if (!m_apiKey.trimmed().isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_apiKey.trimmed().toUtf8());
    return req;
}

QJsonArray LlmClient::messages(const ModelRequest& request) const
{
    QJsonArray out;
    if (!request.system.isEmpty())
        out.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                               {QStringLiteral("content"), request.system}});

    QStringList images;
    for (const QString& b64 : request.imagesBase64)
        if (!b64.trimmed().isEmpty())
            images << b64.trimmed();

    QJsonObject user{{QStringLiteral("role"), QStringLiteral("user")}};
    if (images.isEmpty()) {
        user.insert(QStringLiteral("content"), request.user);
    } else if (m_provider == Provider::Ollama) {
        user.insert(QStringLiteral("content"), request.user);
        user.insert(QStringLiteral("images"), QJsonArray::fromStringList(images));
    } else {
        QJsonArray parts;
        parts.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                 {QStringLiteral("text"), request.user}});
        for (const QString& b64 : images)
            parts.append(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("image_url")},
                {QStringLiteral("image_url"),
                 QJsonObject{{QStringLiteral("url"), QStringLiteral("data:image/png;base64,") + b64}}}});
        user.insert(QStringLiteral("content"), parts);
    }
    out.append(user);
    return out;
}

QJsonObject LlmClient::requestBody(const ModelRequest& request) const
{
    QVariantMap merged = m_options;
    for (auto it = request.options.begin(); it != request.options.end(); ++it)
        merged.insert(it.key(), it.value());

    QJsonObject body;
    body.insert(QStringLiteral("model"), m_model);
    body.insert(QStringLiteral("stream"), request.stream);
    body.insert(QStringLiteral("messages"), messages(request));

    if (m_provider == Provider::Ollama) {
        QJsonObject options;
        for (auto it = merged.begin(); it != merged.end(); ++it) {
            if (!isOllamaOption(it.key()))
                continue;
            if (it.key() == QLatin1String("stop")) {
                const QJsonValue stop = stopValueToJson(it.value());
                if (!stop.isUndefined() && !stop.isNull())
                    options.insert(it.key(), stop);
                continue;
            }
            if (it.key() == QLatin1String("seed") && it.value().toLongLong() == 0)
                continue;  // 0 means "random": leave it to the server
            options.insert(it.key(), QJsonValue::fromVariant(it.value()));
        }
        if (!options.isEmpty())
            body.insert(QStringLiteral("options"), options);
        return body;
    }

    // OpenAI-compatible: the options are top-level fields, with OpenAI names where they differ.
    const auto copyDouble = [&](const char* key) {
        if (merged.contains(QLatin1String(key)))
            body.insert(QLatin1String(key), merged.value(QLatin1String(key)).toDouble());
    };
    const auto copyInt = [&](const char* key) {
        if (merged.contains(QLatin1String(key)))
            body.insert(QLatin1String(key), merged.value(QLatin1String(key)).toInt());
    };
    copyDouble("temperature");
    copyDouble("top_p");
    copyInt("top_k");
    copyDouble("presence_penalty");
    copyDouble("frequency_penalty");
    copyDouble("repeat_penalty");
    copyInt("repeat_last_n");
    const qlonglong seed = merged.value(QStringLiteral("seed")).toLongLong();
    if (seed != 0)
        body.insert(QStringLiteral("seed"), seed);
    const int maxTokens = merged.value(QStringLiteral("num_predict"), -1).toInt();
    if (maxTokens >= 0)
        body.insert(QStringLiteral("max_tokens"), maxTokens);
    if (merged.contains(QStringLiteral("stop"))) {
        const QJsonValue stop = stopValueToJson(merged.value(QStringLiteral("stop")));
        if (!stop.isUndefined() && !stop.isNull())
            body.insert(QStringLiteral("stop"), stop);
    }
    return body;
}

QString LlmClient::parseAnswer(const QByteArray& data, QString* error) const
{
    const QJsonObject json = QJsonDocument::fromJson(data).object();
    const QString serverError = errorFromJson(json);
    if (!serverError.isEmpty()) {
        *error = serverError;
        return QString();
    }
    if (m_provider == Provider::OpenAiCompatible) {
        const QJsonArray choices = json.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty())
            return QString();
        return openAiContent(choices.first().toObject().value(QStringLiteral("message"))
                                     .toObject().value(QStringLiteral("content")));
    }
    return json.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
}

void LlmClient::handleStreamData(QNetworkReply* net, ModelReply* reply, QByteArray* buffer)
{
    *buffer += net->readAll();
    for (int nl = buffer->indexOf('\n'); nl >= 0; nl = buffer->indexOf('\n')) {
        QByteArray line = buffer->left(nl).trimmed();
        buffer->remove(0, nl + 1);
        if (line.isEmpty())
            continue;
        if (m_provider == Provider::OpenAiCompatible) {
            // Server-sent events: "data: {...}", ending with "data: [DONE]".
            if (!line.startsWith("data:"))
                continue;
            line = line.mid(5).trimmed();
            if (line == "[DONE]")
                continue;
            const QJsonArray choices = QJsonDocument::fromJson(line).object()
                                               .value(QStringLiteral("choices")).toArray();
            if (!choices.isEmpty())
                reply->appendText(openAiContent(choices.first().toObject()
                                                        .value(QStringLiteral("delta")).toObject()
                                                        .value(QStringLiteral("content"))));
            continue;
        }
        // Ollama: one JSON object per line.
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        const QString serverError = errorFromJson(o);
        if (!serverError.isEmpty()) {
            reply->finishWithError(serverError);
            return;
        }
        reply->appendText(o.value(QStringLiteral("message")).toObject()
                                  .value(QStringLiteral("content")).toString());
    }
}

ModelReply* LlmClient::send(const ModelRequest& request)
{
    auto* reply = new ModelReply(this);
    if (m_model.trimmed().isEmpty()) {
        // Reported after the caller has connected to the reply.
        QTimer::singleShot(0, reply, [reply]() {
            reply->finishWithError(QCoreApplication::translate("LlmClient", "No model is selected."));
        });
        return reply;
    }

    const QJsonObject body = requestBody(request);
    m_lastBody = body;
    QNetworkRequest req = httpRequest(m_provider == Provider::OpenAiCompatible ? QStringLiteral("/v1/chat/completions")
                                                                         : QStringLiteral("/api/chat"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QPointer<QNetworkReply> net = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    net->setParent(reply);  // goes away with the reply

    reply->setAbortHandler([net]() {
        if (net)
            net->abort();
    });

    if (request.timeoutMs > 0) {
        auto* timer = new QTimer(reply);
        timer->setSingleShot(true);
        const int seconds = qMax(1, request.timeoutMs / 1000);
        connect(timer, &QTimer::timeout, reply, [reply, net, seconds]() {
            // Finish first, so the abort below does not report itself as the reason.
            reply->finishWithError(QCoreApplication::translate("LlmClient", "No answer within %1 s.").arg(seconds));
            if (net)
                net->abort();
        });
        timer->start(request.timeoutMs);
    }

    if (request.stream) {
        auto buffer = std::make_shared<QByteArray>();
        connect(net, &QNetworkReply::readyRead, reply, [this, net, reply, buffer]() {
            if (net)
                handleStreamData(net, reply, buffer.get());
        });
    }

    const bool stream = request.stream;
    connect(net, &QNetworkReply::finished, reply, [this, net, reply, stream]() {
        if (!net || reply->isFinished())
            return;
        if (net->error() != QNetworkReply::NoError) {
            // The server may explain the failure in the body.
            QString detail;
            parseAnswer(net->readAll(), &detail);
            reply->finishWithError(detail.isEmpty() ? net->errorString() : detail);
            return;
        }
        if (stream) {
            reply->finishWithText(QString());
            return;
        }
        QString error;
        const QString text = parseAnswer(net->readAll(), &error);
        if (!error.isEmpty())
            reply->finishWithError(error);
        else
            reply->finishWithText(text);
    });
    return reply;
}

void LlmClient::fetchModels(QObject* context,
                            std::function<void(const QStringList&, const QString&)> done)
{
    const bool openAi = m_provider == Provider::OpenAiCompatible;
    QNetworkRequest req = httpRequest(openAi ? QStringLiteral("/v1/models") : QStringLiteral("/api/tags"));
    req.setTransferTimeout(8000);
    QNetworkReply* net = m_nam.get(req);
    connect(net, &QNetworkReply::finished, context, [this, net, openAi, context, done]() {
        net->deleteLater();
        if (net->error() != QNetworkReply::NoError) {
            done(QStringList(), net->errorString());
            return;
        }
        const QJsonObject json = QJsonDocument::fromJson(net->readAll()).object();
        QStringList models;
        if (!openAi) {
            for (const QJsonValue& v : json.value(QStringLiteral("models")).toArray()) {
                const QString name = v.toObject().value(QStringLiteral("name")).toString().trimmed();
                if (!name.isEmpty())
                    models << name;
            }
            done(models, QString());
            return;
        }
        QJsonArray list = json.value(QStringLiteral("data")).toArray();
        if (list.isEmpty())
            list = json.value(QStringLiteral("models")).toArray();
        for (const QJsonValue& v : list) {
            const QJsonObject m = v.toObject();
            QString id = m.value(QStringLiteral("id")).toString().trimmed();
            if (id.isEmpty())
                id = m.value(QStringLiteral("name")).toString().trimmed();
            if (!id.isEmpty() && !models.contains(id))
                models << id;
        }
        if (!models.isEmpty()) {
            done(models, QString());
            return;
        }
        // llama.cpp started with one model and no list: /props names it.
        QNetworkRequest propsReq = httpRequest(QStringLiteral("/props"));
        propsReq.setTransferTimeout(8000);
        QNetworkReply* props = m_nam.get(propsReq);
        connect(props, &QNetworkReply::finished, context, [props, done]() {
            props->deleteLater();
            const QJsonObject p = QJsonDocument::fromJson(props->readAll()).object();
            QString id = p.value(QStringLiteral("model")).toString().trimmed();
            if (id.isEmpty())
                id = p.value(QStringLiteral("model_alias")).toString().trimmed();
            if (id.isEmpty())
                id = QFileInfo(p.value(QStringLiteral("model_path")).toString().trimmed()).fileName();
            done(id.isEmpty() ? QStringList() : QStringList{id},
                 props->error() == QNetworkReply::NoError ? QString() : props->errorString());
        });
    });
}
