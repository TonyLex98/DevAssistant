#pragma once
#include <QJsonDocument>
#include <QVector>
#include "core/commands/Command.h"

// Reads the commands in a model's answer: a JSON array, single objects, objects inside prose
// or Markdown fences, with common mistakes repaired; as a last resort one-line commands such
// as "OPEN_FILE path=src/main.cpp". The caller owns the returned commands.
class CommandParser
{
public:
    static QVector<Command*> parse(const QByteArray& json);

    // The JSON in an answer as one array ("[...]"), or empty if there is none.
    static QByteArray extractJson(const QByteArray& answer);

    // Serializes commands back to JSON
    static QByteArray toJson(const QVector<Command*>& cmds);
};
