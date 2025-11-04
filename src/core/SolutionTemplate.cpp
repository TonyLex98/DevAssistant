// SolutionTemplate.cpp
#include "SolutionTemplate.h"
#include <QJsonArray>
#include <QJsonArray>
#include <QJsonDocument>

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
    
    // Serialize positions
    QJsonObject positionsObj;
    for (auto it = m_positions.constBegin(); it != m_positions.constEnd(); ++it) {
        QJsonObject posObj;
        posObj["x"] = it.value().x();
        posObj["y"] = it.value().y();
        positionsObj[it.key()] = posObj;
    }
    obj["positions"] = positionsObj;
    
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
    
    // Deserialize positions
    QJsonObject positionsObj = obj.value("positions").toObject();
    for (auto it = positionsObj.constBegin(); it != positionsObj.constEnd(); ++it) {
        QJsonObject posObj = it.value().toObject();
        QPointF pos(posObj.value("x").toDouble(), posObj.value("y").toDouble());
        tmpl.m_positions.insert(it.key(), pos);
    }
    
    return tmpl;
}

void SolutionTemplate::removeSequenceByPath(const QString& sequencePath)
{
    m_sequences.removeAll(sequencePath);
}