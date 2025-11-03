// SolutionTemplate.cpp
#include "SolutionTemplate.h"
#include <QJsonArray>

QJsonObject SolutionTemplate::toJson() const
{
    QJsonObject obj;
    obj["name"] = m_name;
    obj["description"] = m_description;
    obj["imagePath"] = m_imagePath;
    obj["mainSequencePath"] = m_mainSequencePath;
    
    QJsonArray seqArray;
    for (const QString& seqPath : m_sequences) {
        seqArray.append(seqPath);
    }
    obj["sequences"] = seqArray;
    
    return obj;
}

SolutionTemplate SolutionTemplate::fromJson(const QJsonObject& obj)
{
    SolutionTemplate tmpl;
    tmpl.m_name = obj.value("name").toString();
    tmpl.m_description = obj.value("description").toString();
    tmpl.m_imagePath = obj.value("imagePath").toString();
    tmpl.m_mainSequencePath = obj.value("mainSequencePath").toString();
    
    QJsonArray seqArray = obj.value("sequences").toArray();
    for (const QJsonValue& val : seqArray) {
        tmpl.m_sequences.append(val.toString());
    }
    
    return tmpl;
}