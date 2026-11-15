#pragma once
// A tiny HTTP server for tests that plays a model server: it records each request and
// answers with a prepared body, all at once or in pieces (streaming).

#include <QByteArray>
#include <memory>
#include <QList>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

class FakeModelServer : public QObject
{
public:
    struct Response
    {
        int status = 200;
        QList<QByteArray> parts;  // sent one after another, 20 ms apart
        bool hang = false;        // never answer (for timeouts)
    };
    struct Request
    {
        QByteArray method;
        QByteArray path;
        QByteArray headers;
        QByteArray body;
    };

    FakeModelServer() { m_server.listen(QHostAddress::LocalHost); connect(&m_server, &QTcpServer::newConnection, this, &FakeModelServer::accept); }

    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort())); }
    void addResponse(const Response& r) { m_responses << r; }
    void addJson(const QByteArray& json, int status = 200) { addResponse({status, {json}, false}); }
    const QList<Request>& requests() const { return m_requests; }

private:
    void accept()
    {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
                *buffer += socket->readAll();
                const int headerEnd = buffer->indexOf("\r\n\r\n");
                if (headerEnd < 0)
                    return;
                const QByteArray head = buffer->left(headerEnd);
                int length = 0;
                for (const QByteArray& line : head.split('\n')) {
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(15).trimmed().toInt();
                }
                if (buffer->size() < headerEnd + 4 + length)
                    return;
                const QList<QByteArray> first = head.split('\n').first().split(' ');
                m_requests << Request{first.value(0), first.value(1), head, buffer->mid(headerEnd + 4, length)};
                buffer->clear();
                respond(socket);
            });
        }
    }

    void respond(QTcpSocket* socket)
    {
        const Response r = m_responses.isEmpty() ? Response{404, {"{}"}, false} : m_responses.takeFirst();
        if (r.hang)
            return;
        QByteArray body;
        for (const QByteArray& p : r.parts)
            body += p;
        const QByteArray head = "HTTP/1.1 " + QByteArray::number(r.status) + " X\r\n"
                                "Content-Type: application/json\r\n"
                                "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                "Connection: close\r\n\r\n";
        socket->write(head);
        QPointer<QTcpSocket> guard(socket);
        int delay = 0;
        for (const QByteArray& part : r.parts) {
            QTimer::singleShot(delay, this, [guard, part]() {
                if (guard)
                    guard->write(part);
            });
            delay += 20;
        }
        QTimer::singleShot(delay, this, [guard]() {
            if (guard)
                guard->disconnectFromHost();
        });
    }

    QTcpServer m_server;
    QList<Response> m_responses;
    QList<Request> m_requests;
};
