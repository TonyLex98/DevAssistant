// CommandRegistry.h
#pragma once
#include <QVector>
#include "Command.h"

class CommandRegistry
{
public:
    /* Возвращает шаблоны всех команд (для генерации списка). */
    static QVector<Command*> all();

    /* Создаёт команду из JSON‑объекта. */
    static Command* create(const QJsonObject& obj);
};
