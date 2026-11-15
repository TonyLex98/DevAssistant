// CommandParser.h
#pragma once
#include <QJsonDocument>
#include <QVector>
#include "core/commands/Command.h"

class CommandParser
{
public:
    // Parses a JSON array of objects and returns the commands
    static QVector<Command*> parse(const QByteArray& json);

    // Serializes commands back to JSON
    static QByteArray toJson(const QVector<Command*>& cmds);
};
