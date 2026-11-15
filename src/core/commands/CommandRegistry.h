// CommandRegistry.h
#pragma once
#include <QVector>
#include "core/commands/Command.h"

class CommandRegistry
{
public:
    /* Prototypes of all commands (for building the list). */
    static QVector<Command*> all();

    /* Creates a command from a JSON object. */
    static Command* create(const QJsonObject& obj);
};
