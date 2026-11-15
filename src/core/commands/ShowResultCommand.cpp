#include "core/commands/ShowResultCommand.h"

ShowResultCommand::ShowResultCommand(const QString& content,
                                     const QString& title,
                                     const QString& descr)
{
    m_params["content"] = content;
    if (!title.isEmpty())
        m_params["title"] = title;
    m_description = descr.isEmpty()
        ? QString("Shows a result or message to the user. "
                  "Use this to present final answers, summaries, or important information. "
                  "Example: { \"command\": \"SHOW_RESULT\", \"params\": { \"title\": \"Analysis Complete\", \"content\": \"The bug was found in line 42...\" } }")
        : descr;
}

bool ShowResultCommand::apply(DevAssManager*, QString*)
{
    return true; // no filesystem changes; handled by MainWindow hook
}

QJsonObject ShowResultCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = m_description;
    return obj;
}

QString ShowResultCommand::getResult() const
{
    // Return the content as the result - this will be added to the context
    QString result = m_params.value("content").toString();
    QString title = m_params.value("title").toString();
    
    if (!title.isEmpty()) {
        result = QString("%1\n%2").arg(title, result);
    }
    
    return result;
}
