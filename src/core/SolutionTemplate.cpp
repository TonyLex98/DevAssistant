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
    
    // Serialize sequence targets (directed links)
    if (!m_sequenceTargets.isEmpty()) {
        obj["sequenceTargets"] = m_sequenceTargets;
    }
    
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
    
    // Deserialize sequence targets (directed links)
    if (obj.contains("sequenceTargets")) {
        tmpl.m_sequenceTargets = obj.value("sequenceTargets").toObject();
    } else if (obj.contains("connections")) {
        // Older templates list links as [{"from": a, "to": b}]; turn them into a -> [b, ...].
        for (const QJsonValue& v : obj.value("connections").toArray()) {
            const QJsonObject link = v.toObject();
            const QString from = link.value("from").toString();
            const QString to = link.value("to").toString();
            if (from.isEmpty() || to.isEmpty())
                continue;
            QJsonArray targets = tmpl.m_sequenceTargets.value(from).toArray();
            targets.append(to);
            tmpl.m_sequenceTargets.insert(from, targets);
        }
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

void SolutionTemplate::setTargetsForSequence(const QString& seqName, const QJsonArray& targets) 
{
    m_sequenceTargets[seqName] = targets;
}

QJsonArray SolutionTemplate::getTargetsForSequence(const QString& seqName) const 
{
    if (m_sequenceTargets.contains(seqName)) {
        return m_sequenceTargets.value(seqName).toArray();
    }
    return QJsonArray();  // Return empty array if sequence has no targets
}

void SolutionTemplate::removeSequenceByPath(const QString& sequencePath)
{
    m_sequences.removeAll(sequencePath);
}