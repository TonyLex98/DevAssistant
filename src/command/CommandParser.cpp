// CommandParser.cpp
#include "CommandParser.h"
#include "CommandRegistry.h"

QVector<Command*> CommandParser::parse(const QByteArray& json)
{
    QVector<Command*> res;
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) return res;
    QJsonArray arr = doc.array();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        Command* cmd = CommandRegistry::create(v.toObject());
        if (cmd) res.append(cmd);
    }
    return res;
}

QByteArray CommandParser::toJson(const QVector<Command*>& cmds)
{
    QJsonArray arr;
    for (Command* c : cmds)
        arr.append(c->toJson());
    QJsonDocument doc(arr);
    return doc.toJson(QJsonDocument::Indented);
}
