// LlmClient against a fake model server: request bodies for both protocols, answers whole
// and streamed, errors, timeouts, cancellation and the model list.

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>
#include "FakeModelServer.h"
#include "core/model/LlmClient.h"

class TestLlmClient : public QObject
{
    Q_OBJECT

private slots:
    void ollamaRequestBody();
    void openAiRequestBody();
    void imagesGoWhereEachProtocolWantsThem();
    void ollamaAnswer();
    void ollamaStream();
    void openAiStream();
    void serverErrorIsReported();
    void timeout();
    void abort();
    void noModelSelected();
    void apiKeyIsSent();
    void fetchModels();

private:
    static bool finish(ModelReply* reply) { QSignalSpy spy(reply, &ModelReply::finished); return reply->isFinished() || spy.wait(5000); }
};

void TestLlmClient::ollamaRequestBody()
{
    LlmClient client;
    client.configure(QStringLiteral("qwen"), {{"temperature", 0.3}, {"num_ctx", 8192}, {"mirostat", 1}, {"seed", 0}});
    ModelRequest request;
    request.system = QStringLiteral("be brief");
    request.user = QStringLiteral("hello");
    request.options = {{"temperature", 0.1}};
    const QJsonObject body = client.requestBody(request);

    QCOMPARE(body.value("model").toString(), QStringLiteral("qwen"));
    QCOMPARE(body.value("stream").toBool(), false);
    const QJsonArray messages = body.value("messages").toArray();
    QCOMPARE(messages.size(), 2);
    QCOMPARE(messages.at(0).toObject().value("role").toString(), QStringLiteral("system"));
    QCOMPARE(messages.at(1).toObject().value("content").toString(), QStringLiteral("hello"));
    const QJsonObject options = body.value("options").toObject();
    QCOMPARE(options.value("temperature").toDouble(), 0.1);  // the request's own option wins
    QCOMPARE(options.value("num_ctx").toInt(), 8192);
    QVERIFY(!options.contains("mirostat"));  // not an Ollama option
    QVERIFY(!options.contains("seed"));      // 0 = random: left to the server
}

void TestLlmClient::openAiRequestBody()
{
    LlmClient client;
    client.setProvider(LlmClient::Provider::OpenAiCompatible);
    client.configure(QStringLiteral("local"), {{"temperature", 0.3}, {"num_predict", 256}, {"stop", QStringList{"END"}}});
    ModelRequest request;
    request.user = QStringLiteral("hello");
    request.stream = true;
    const QJsonObject body = client.requestBody(request);

    QCOMPARE(body.value("stream").toBool(), true);
    QCOMPARE(body.value("temperature").toDouble(), 0.3);
    QCOMPARE(body.value("max_tokens").toInt(), 256);
    QCOMPARE(body.value("stop").toArray(), QJsonArray{"END"});
    QVERIFY(!body.contains("options"));
    QCOMPARE(body.value("messages").toArray().size(), 1);  // no system message when there is none
}

void TestLlmClient::imagesGoWhereEachProtocolWantsThem()
{
    ModelRequest request;
    request.user = QStringLiteral("what is this?");
    request.imagesBase64 = QStringList{QStringLiteral("AAAA")};

    LlmClient ollama;
    ollama.setModel(QStringLiteral("llava"));
    const QJsonObject user = ollama.requestBody(request).value("messages").toArray().last().toObject();
    QCOMPARE(user.value("images").toArray(), QJsonArray{"AAAA"});

    LlmClient openAi;
    openAi.setProvider(LlmClient::Provider::OpenAiCompatible);
    openAi.setModel(QStringLiteral("llava"));
    const QJsonArray parts = openAi.requestBody(request).value("messages").toArray().last().toObject()
                                     .value("content").toArray();
    QCOMPARE(parts.size(), 2);
    QCOMPARE(parts.at(1).toObject().value("image_url").toObject().value("url").toString(),
             QStringLiteral("data:image/png;base64,AAAA"));
}

void TestLlmClient::ollamaAnswer()
{
    FakeModelServer server;
    server.addJson(R"({"message":{"role":"assistant","content":"Hi!"},"done":true})");
    LlmClient client;
    client.setEndpoint(server.url());
    client.setModel(QStringLiteral("qwen"));
    ModelRequest request;
    request.user = QStringLiteral("hello");

    ModelReply* reply = client.send(request);
    QVERIFY(finish(reply));
    QVERIFY2(!reply->hasError(), qPrintable(reply->errorString()));
    QCOMPARE(reply->text(), QStringLiteral("Hi!"));
    QCOMPARE(server.requests().size(), 1);
    QCOMPARE(server.requests().first().path, QByteArray("/api/chat"));
    QCOMPARE(QJsonDocument::fromJson(server.requests().first().body).object().value("model").toString(), QStringLiteral("qwen"));
}

void TestLlmClient::ollamaStream()
{
    FakeModelServer server;
    server.addResponse({200, {R"({"message":{"content":"Hel"}})" "\n", R"({"message":{"content":"lo"}})" "\n",
                              R"({"message":{"content":""},"done":true})" "\n"}, false});
    LlmClient client;
    client.setEndpoint(server.url());
    client.setModel(QStringLiteral("qwen"));
    ModelRequest request;
    request.user = QStringLiteral("hello");
    request.stream = true;

    ModelReply* reply = client.send(request);
    QStringList chunks;
    connect(reply, &ModelReply::chunk, this, [&chunks](const QString& c) { chunks << c; });
    QVERIFY(finish(reply));
    QVERIFY2(!reply->hasError(), qPrintable(reply->errorString()));
    QCOMPARE(reply->text(), QStringLiteral("Hello"));
    QCOMPARE(chunks.join(QString()), QStringLiteral("Hello"));
}

void TestLlmClient::openAiStream()
{
    FakeModelServer server;
    server.addResponse({200, {"data: {\"choices\":[{\"delta\":{\"content\":\"Hel\"}}]}\n\n",
                              "data: {\"choices\":[{\"delta\":{\"content\":\"lo\"}}]}\n\n",
                              "data: [DONE]\n\n"}, false});
    LlmClient client;
    client.setProvider(LlmClient::Provider::OpenAiCompatible);
    client.setEndpoint(server.url());
    client.setModel(QStringLiteral("local"));
    ModelRequest request;
    request.user = QStringLiteral("hello");
    request.stream = true;

    ModelReply* reply = client.send(request);
    QVERIFY(finish(reply));
    QVERIFY2(!reply->hasError(), qPrintable(reply->errorString()));
    QCOMPARE(reply->text(), QStringLiteral("Hello"));
    QCOMPARE(server.requests().first().path, QByteArray("/v1/chat/completions"));
}

void TestLlmClient::serverErrorIsReported()
{
    FakeModelServer server;
    server.addJson(R"({"error":"model 'nope' not found"})", 404);
    LlmClient client;
    client.setEndpoint(server.url());
    client.setModel(QStringLiteral("nope"));
    ModelRequest request;
    request.user = QStringLiteral("hello");

    ModelReply* reply = client.send(request);
    QVERIFY(finish(reply));
    QVERIFY(reply->hasError());
    QVERIFY2(reply->errorString().contains(QStringLiteral("not found")), qPrintable(reply->errorString()));
}

void TestLlmClient::timeout()
{
    FakeModelServer server;
    server.addResponse({200, {}, true});
    LlmClient client;
    client.setEndpoint(server.url());
    client.setModel(QStringLiteral("slow"));
    ModelRequest request;
    request.user = QStringLiteral("hello");
    request.timeoutMs = 300;

    ModelReply* reply = client.send(request);
    QVERIFY(finish(reply));
    QVERIFY(reply->hasError());
    QVERIFY(!reply->isCancelled());
    QVERIFY2(reply->errorString().contains(QStringLiteral("1 s")), qPrintable(reply->errorString()));
}

void TestLlmClient::abort()
{
    FakeModelServer server;
    server.addResponse({200, {}, true});
    LlmClient client;
    client.setEndpoint(server.url());
    client.setModel(QStringLiteral("slow"));
    ModelRequest request;
    request.user = QStringLiteral("hello");

    ModelReply* reply = client.send(request);
    QSignalSpy finished(reply, &ModelReply::finished);
    reply->abort();
    QCOMPARE(finished.count(), 1);
    QVERIFY(reply->isCancelled());
    QVERIFY(reply->hasError());
    reply->finishWithText(QStringLiteral("late"));  // the outcome does not change any more
    QCOMPARE(finished.count(), 1);
    QVERIFY(reply->text().isEmpty());
}

void TestLlmClient::noModelSelected()
{
    LlmClient client;
    ModelRequest request;
    request.user = QStringLiteral("hello");
    ModelReply* reply = client.send(request);
    QVERIFY(!reply->isFinished());  // reported after the caller has connected
    QVERIFY(finish(reply));
    QVERIFY(reply->hasError());
}

void TestLlmClient::apiKeyIsSent()
{
    FakeModelServer server;
    server.addJson(R"({"choices":[{"message":{"content":"ok"}}]})");
    LlmClient client;
    client.setProvider(LlmClient::Provider::OpenAiCompatible);
    client.setEndpoint(server.url());
    client.setModel(QStringLiteral("m"));
    client.setApiKey(QStringLiteral("secret"));
    ModelRequest request;
    request.user = QStringLiteral("hello");

    ModelReply* reply = client.send(request);
    QVERIFY(finish(reply));
    QCOMPARE(reply->text(), QStringLiteral("ok"));
    QVERIFY(server.requests().first().headers.contains("Bearer secret"));
}

void TestLlmClient::fetchModels()
{
    FakeModelServer server;
    server.addJson(R"({"models":[{"name":"qwen2.5-coder:7b"},{"name":"llava"}]})");
    LlmClient client;
    client.setEndpoint(server.url());

    QStringList models;
    bool done = false;
    client.fetchModels(this, [&](const QStringList& list, const QString&) { models = list; done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
    QCOMPARE(models, (QStringList{"qwen2.5-coder:7b", "llava"}));
    QCOMPARE(server.requests().first().path, QByteArray("/api/tags"));
}

QTEST_MAIN(TestLlmClient)
#include "tst_llmclient.moc"
