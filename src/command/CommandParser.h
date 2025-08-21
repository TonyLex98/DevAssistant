// CommandParser.h
#pragma once
#include <QJsonDocument>
#include <QVector>
#include "Command.h"

class CommandParser
{
public:
    // Парсит массив JSON‑объектов и возвращает список команд
    static QVector<Command*> parse(const QByteArray& json);

    // Вспомогательный: сериализует список команд обратно в JSON
    static QByteArray toJson(const QVector<Command*>& cmds);
};
