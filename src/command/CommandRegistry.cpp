// CommandRegistry.cpp
#include "CommandRegistry.h"
#include "CreateFileCommand.h"
#include "ReplaceInFileCommand.h"
#include "DeleteFileCommand.h"
#include "RenameFileCommand.h"
#include "EditInRangeCommand.h"
#include "WriteDescriptionCommand.h"
#include "OpenFileContextCommand.h"
#include "OpenFileRegionCommand.h"
#include "AskUserCommand.h"
#include "RunPythonCommand.h"
#include "RunShellCommand.h"
#include "SearchWebCommand.h"
#include "SearchProjectCommand.h"
#include "ShowResultCommand.h"
#include "CallSequenceCommand.h"
#include <QJsonArray>
#include <QJsonDocument>

static QJsonObject parseJsonObject(const QByteArray& bytes)
{
    if (bytes.isEmpty())
        return QJsonObject();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject())
        return doc.object();
    return QJsonObject();
}

static QJsonObject paramsObject(const QJsonValue& value)
{
    if (value.isObject())
        return value.toObject();

    if (!value.isString())
        return QJsonObject();

    QString raw = value.toString().trimmed();
    QJsonObject obj = parseJsonObject(raw.toUtf8());
    if (!obj.isEmpty())
        return obj;

    if (!raw.startsWith('{') && !raw.endsWith('}')) {
        QByteArray wrapped = "{" + raw.toUtf8() + "}";
        obj = parseJsonObject(wrapped);
        if (!obj.isEmpty())
            return obj;
    }
    return QJsonObject();
}


QVector<Command*> CommandRegistry::all()
{
    return {
        new AskUserCommand(QString()),
        new OpenFileRegionCommand(QString()),
        new CreateFileCommand(QString(), QByteArray()),
        new ReplaceInFileCommand(QString(), QByteArray()),
        new DeleteFileCommand(QString()),
        new RenameFileCommand(QString(), QString()),
        new EditInRangeCommand(QString(), 0, 0, QByteArray(), QJsonArray()),
        new WriteDescriptionCommand(QString(), QString()),
        new OpenFileContextCommand(QString()),
        new RunPythonCommand(QString(), QByteArray()),
        new RunShellCommand(QString()),
        new SearchWebCommand(QString()),
        new SearchProjectCommand(QString()),
        new ShowResultCommand(QString()),
        new CallSequenceCommand(QString())
    };
}

/* ---------- Реализация create() ---------- */
Command* CommandRegistry::create(const QJsonObject& obj)
{
    const QString cmd = obj.value("command").toString();

    if (cmd == "CREATE_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new CreateFileCommand(
            p["path"].toString(),
            p["content"].toString().toUtf8(),
            obj["description"].toString()
        );
    }

    if (cmd == "REPLACE_IN_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new ReplaceInFileCommand(
            p["path"].toString(),
            p["content"].toString().toUtf8(),
            obj["description"].toString()
        );
    }

    if (cmd == "DELETE_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new DeleteFileCommand(
            p["path"].toString(),
            obj["description"].toString()
        );
    }

    if (cmd == "RENAME_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new RenameFileCommand(
            p["oldPath"].toString(),
            p["newPath"].toString(),
            obj["description"].toString()
        );
    }

    if (cmd == "EDIT_IN_RANGE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QJsonArray linesArray = p.value("replacementLines").toArray();
        if (linesArray.isEmpty() && p.contains("lines"))
            linesArray = p.value("lines").toArray();
        return new EditInRangeCommand(
            p["path"].toString(),
            p["startLine"].toInt(),
            p["endLine"].toInt(),
            p.value("content").toString().toUtf8(),
            linesArray,
            obj["description"].toString()
        );
    }

    if (cmd == "WRITE_DESCRIPTION") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new WriteDescriptionCommand(
            p["sourceFilePath"].toString(),
            p["description"].toString()
        );
    }

    // +++ в CommandRegistry::create()
    if (cmd == "OPEN_FILE_REGION") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new OpenFileRegionCommand(
            p["path"].toString(),
            p.value("region").toString(),
            p.value("startLine").toInt(-1),
            p.value("endLine").toInt(-1),
            obj["description"].toString()
        );
    }

    if (cmd == "OPEN_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new OpenFileContextCommand(
            p["path"].toString(),
            obj["description"].toString()
        );
    }

    if (cmd == "ASK_USER") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new AskUserCommand(p.value("question").toString(), obj["description"].toString());
    }

    // +++ в CommandRegistry::create()
    if (cmd == "RUN_PYTHON") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString scriptPath = p.value("scriptPath").toString();
        if (scriptPath.isEmpty())
            scriptPath = p.value("script_path").toString();
        if (scriptPath.isEmpty())
            scriptPath = p.value("path").toString();

        QString codeStr = p.value("code").toString();
        if (codeStr.isEmpty())
            codeStr = p.value("inlineCode").toString();
        if (codeStr.isEmpty())
            codeStr = p.value("inline_code").toString();
        if (codeStr.isEmpty())
            codeStr = p.value("python").toString();

        return new RunPythonCommand(
            scriptPath,
            codeStr.toUtf8(),
            obj["description"].toString()
        );
    }

    if (cmd == "RUN_SHELL") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new RunShellCommand(
            p.value("command").toString(),
            obj["description"].toString()
        );
    }

    if (cmd == "SEARCH_WEB") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new SearchWebCommand(
            p.value("query").toString(),
            p.value("maxResults").toInt(5),
            obj["description"].toString()
        );
    }

    if (cmd == "SEARCH_PROJECT") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QStringList includes;
        QJsonArray arr = p.value("include").toArray();
        for (const QJsonValue& v : arr)
            includes << v.toString();
        return new SearchProjectCommand(
            p.value("query").toString(),
            p.value("maxMatches").toInt(10),
            p.value("caseSensitive").toBool(false),
            includes,
            obj["description"].toString()
        );
    }

    if (cmd == "SHOW_RESULT") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new ShowResultCommand(
            p.value("content").toString(),
            p.value("title").toString(),
            obj["description"].toString()
        );
    }

    if (cmd == "CALL_SEQUENCE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        return new CallSequenceCommand(
            p["sequenceName"].toString(),
            obj["description"].toString()
        );
    }

    /* Неизвестная команда */
    return nullptr;
}
