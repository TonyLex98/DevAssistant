#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QDebug>
#include <QEventLoop>

class TestLightRagClient : public QObject
{
    Q_OBJECT

public:
    explicit TestLightRagClient(QObject *parent = nullptr)
        : QObject(parent)
        , m_networkManager(new QNetworkAccessManager(this))
    {
    }

    void testConnection() 
    {
        // Test connection to LightRAG server
        QUrl url("http://localhost:9621/v1/models");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        qDebug() << "Sending test request to:" << url.toString();
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.setInterval(5000); // 5 seconds timeout
        connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        timeoutTimer.start();
        loop.exec();
        
        if (timeoutTimer.isActive()) {
            timeoutTimer.stop();
            
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray response = reply->readAll();
                qDebug() << "Server responded with:" << response;
                qDebug() << "Connection to LightRAG server: SUCCESS";
            } else {
                qDebug() << "Network error:" << reply->errorString();
                qDebug() << "Connection to LightRAG server: FAILED";
                qDebug() << "Make sure LightRAG server is running at http://localhost:9621";
            }
        } else {
            qDebug() << "Request timed out. Connection to LightRAG server: FAILED";
            qDebug() << "Make sure LightRAG server is running at http://localhost:9621";
        }
        
        reply->deleteLater();
    }

private:
    QNetworkAccessManager *m_networkManager;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    TestLightRagClient client;
    client.testConnection();

    return 0;
}

#include "main.moc"