// CommandRegistry.cpp
#include "CommandRegistry.h"
#include "CreateFileCommand.h"
#include "ReplaceInFileCommand.h"
#include "DeleteFileCommand.h"
#include "RenameFileCommand.h"
#include "EditInRangeCommand.h"
#include "WriteDescriptionCommand.h"

QVector<Command*> CommandRegistry::all()
{
    return {
        new CreateFileCommand(QString(), QByteArray()),
        new ReplaceInFileCommand(QString(), QByteArray()),
        new DeleteFileCommand(QString()),
        new RenameFileCommand(QString(), QString()),
        new EditInRangeCommand(QString(), 0, 0, QByteArray()),
        new WriteDescriptionCommand(QString(), QString())
    };
}

/* ---------- Реализация create() ---------- */
Command* CommandRegistry::create(const QJsonObject& obj)
{
    const QString cmd = obj.value("command").toString();

    if (cmd == "CREATE_FILE") {
        const QJsonObject p = obj["params"].toObject();
        return new CreateFileCommand(
            p["path"].toString(),
            p["content"].toString().toUtf8(),
            obj["description"].toString()
        );
    }

    if (cmd == "REPLACE_IN_FILE") {
        const QJsonObject p = obj["params"].toObject();
        return new ReplaceInFileCommand(
            p["path"].toString(),
            p["content"].toString().toUtf8(),
            obj["description"].toString()
        );
    }

    if (cmd == "DELETE_FILE") {
        const QJsonObject p = obj["params"].toObject();
        return new DeleteFileCommand(
            p["path"].toString(),
            obj["description"].toString()
        );
    }

    if (cmd == "RENAME_FILE") {
        const QJsonObject p = obj["params"].toObject();
        return new RenameFileCommand(
            p["oldPath"].toString(),
            p["newPath"].toString(),
            obj["description"].toString()
        );
    }

    if (cmd == "EDIT_IN_RANGE") {
        const QJsonObject p = obj["params"].toObject();
        return new EditInRangeCommand(
            p["path"].toString(),
            p["startLine"].toInt(),
            p["endLine"].toInt(),
            p["content"].toString().toUtf8(),
            obj["description"].toString()
        );
    }

    if (cmd == "WRITE_DESCRIPTION") {
        const QJsonObject p = obj["params"].toObject();
        return new WriteDescriptionCommand(
            p["sourceFilePath"].toString(),
            p["description"].toString()
        );
    }

    /* Неизвестная команда */
    return nullptr;
}
