// PromptStep.cpp
#include "PromptStep.h"

QJsonObject PromptStep::toJson() const
{
    QJsonObject o;
    o["template"] = m_template;
    QJsonArray arr;
    for (const QString &c : m_allowedCommands)
        arr.append(c);
    o["allowedCommands"] = arr;

    o["loop"] = m_loop;
    return o;
}

PromptStep PromptStep::fromJson(const QJsonObject &obj)
{
    PromptStep step;
    step.m_template = obj.value("template").toString();

    QJsonArray arr = obj.value("allowedCommands").toArray();
    for (const QJsonValue &v : arr)
        step.m_allowedCommands << v.toString();

    // loop flag (false when absent)
    step.m_loop = obj.value("loop").toBool(false);

    return step;
}
