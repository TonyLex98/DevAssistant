// PromptSequence.cpp
#include "PromptSequence.h"

QJsonObject PromptSequence::toJson() const
{
    QJsonObject o;
    o["name"] = m_name;
    o["description"] = m_description;

    QJsonArray stepsArr;
    for (const PromptStep &s : m_steps)
        stepsArr.append(s.toJson());

    o["steps"] = stepsArr;
    return o;
}

PromptSequence PromptSequence::fromJson(const QJsonObject &obj)
{
    PromptSequence seq;
    seq.m_name = obj.value("name").toString();
    seq.m_description = obj.value("description").toString();

    QJsonArray stepsArr = obj.value("steps").toArray();
    for (const QJsonValue &v : stepsArr) {
        if (!v.isObject()) continue;
        seq.m_steps.append(PromptStep::fromJson(v.toObject()));
    }
    return seq;
}
