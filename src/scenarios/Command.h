// File: Command.h
#pragma once
#include <QJsonObject>
#include <QVector>

struct Command
{
    // тип команды – строка, например "RENAME_FILE"
    QString type;
    // параметры команды – объект JSON
    QJsonObject params;
};
