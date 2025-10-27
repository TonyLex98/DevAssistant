#include "ShowResultCommand.h"

ShowResultCommand::ShowResultCommand(const QString& content,
                                     const QString& title,
                                     const QString& descr)
{
    m_params["content"] = content;
    if (!title.isEmpty())
        m_params["title"] = title;
    m_description = descr.isEmpty()
        ? QString("Показать результат пользователю")
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
