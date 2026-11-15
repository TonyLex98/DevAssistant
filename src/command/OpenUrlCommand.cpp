#include "OpenUrlCommand.h"

#include <QDesktopServices>
#include <QUrl>

OpenUrlCommand::OpenUrlCommand(const QString& url, const QString& descr)
{
    m_params["url"] = url;
    m_description = descr.isEmpty()
        ? QStringLiteral("Opens URL in the system browser. "
                         "Example: { \"command\": \"OPEN_URL\", \"params\": { \"url\": \"https://example.com\" } }")
        : descr;
}

bool OpenUrlCommand::apply(DevAssManager* devAss, QString* err)
{
    Q_UNUSED(devAss)

    QString rawUrl = m_params.value("url").toString().trimmed();
    if (rawUrl.isEmpty()) {
        if (err) *err = "OPEN_URL: url is empty";
        return false;
    }

    QUrl url = QUrl::fromUserInput(rawUrl);
    if (!url.isValid()) {
        if (err) *err = QString("OPEN_URL: invalid url: %1").arg(rawUrl);
        return false;
    }
    // Only web pages: a file:// or custom-scheme URL would let the model start any program
    // through the shell's URL handlers.
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
        if (err) *err = QString("OPEN_URL: only http and https links can be opened, got %1").arg(rawUrl);
        return false;
    }

    m_opened = QDesktopServices::openUrl(url);
    if (!m_opened) {
        if (err) *err = QString("OPEN_URL: cannot open %1").arg(url.toString());
        return false;
    }
    m_params["url"] = url.toString();
    return true;
}

QJsonObject OpenUrlCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString OpenUrlCommand::getResult() const
{
    QString result = QString("OPEN_URL: %1\n[status]\n%2")
                         .arg(m_params.value("url").toString(),
                              m_opened ? "Opened" : "Failed");
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

