#include "core/commands/ClipboardSetCommand.h"

#include <QClipboard>
#include <QGuiApplication>

ClipboardSetCommand::ClipboardSetCommand(const QString& text, const QString& descr)
{
    m_params["text"] = text;
    m_description = descr.isEmpty()
        ? QStringLiteral("Sets the system clipboard text. "
                         "Example: { \"command\": \"CLIPBOARD_SET\", \"params\": { \"text\": \"Hello\" } }")
        : descr;
}

bool ClipboardSetCommand::apply(DevAssManager* devAss, QString* err)
{
    Q_UNUSED(devAss)

    QClipboard* cb = QGuiApplication::clipboard();
    if (!cb) {
        if (err) *err = "CLIPBOARD_SET: clipboard is unavailable";
        return false;
    }

    const QString text = m_params.value("text").toString();
    cb->setText(text);
    m_ok = (cb->text() == text);
    if (!m_ok && err)
        *err = "CLIPBOARD_SET: failed to verify clipboard content";
    return m_ok;
}

QJsonObject ClipboardSetCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString ClipboardSetCommand::getResult() const
{
    QString result = QString("CLIPBOARD_SET\n[status]\n%1")
                         .arg(m_ok ? "Clipboard updated" : "Clipboard update failed");
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

