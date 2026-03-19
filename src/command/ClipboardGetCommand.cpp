#include "ClipboardGetCommand.h"

#include <QClipboard>
#include <QGuiApplication>

ClipboardGetCommand::ClipboardGetCommand(int maxChars, const QString& descr)
{
    m_params["maxChars"] = maxChars;
    m_description = descr.isEmpty()
        ? QStringLiteral("Reads text from the system clipboard. "
                         "Example: { \"command\": \"CLIPBOARD_GET\", \"params\": { \"maxChars\": 2000 } }")
        : descr;
}

bool ClipboardGetCommand::apply(DevAssManager* devAss, QString* err)
{
    Q_UNUSED(devAss)

    QClipboard* cb = QGuiApplication::clipboard();
    if (!cb) {
        if (err) *err = "CLIPBOARD_GET: clipboard is unavailable";
        return false;
    }

    m_value = cb->text();
    const int maxChars = m_params.value("maxChars").toInt(4000);
    if (maxChars > 0 && m_value.size() > maxChars)
        m_value = m_value.left(maxChars) + "\n... [truncated]";
    return true;
}

QJsonObject ClipboardGetCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString ClipboardGetCommand::getResult() const
{
    QString result = "CLIPBOARD_GET\n[clipboard]\n";
    result += m_value.isEmpty() ? "<empty>" : m_value;
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

