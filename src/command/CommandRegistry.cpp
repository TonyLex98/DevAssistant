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
#include "OpenSequenceCommand.h"
#include "SearchProjectCaseInsensitiveCommand.h"
#include "ListFilesInFolderCommand.h"
#include "ReasoningCommand.h"
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
        new SearchProjectCaseInsensitiveCommand(QString()),
        new ShowResultCommand(QString()),
        new CallSequenceCommand(QString()),
        new OpenSequenceCommand(QString()),
        new ListFilesInFolderCommand(QString()),
        new ReasoningCommand(QString(), QString(), QJsonObject())
    };
}

/* ---------- Реализация create() ---------- */
Command* CommandRegistry::create(const QJsonObject& obj)
{
    QString cmd = obj.value("command").toString();
    // Поддержка альтернативного формата, где используется "action" вместо "command"
    if (cmd.isEmpty()) {
        cmd = obj.value("action").toString();
    }

    if (cmd == "CREATE_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString path = p["path"].toString();
        QByteArray content = p["content"].toString().toUtf8();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата (для случаев, когда нейросеть возвращает в другом формате)
        if (path.isEmpty()) {
            path = obj["file_path"].toString();  // Поддержка "file_path" вместо "path" в params
        }
        if (content.isEmpty()) {
            content = obj["content"].toString().toUtf8();  // Поддержка "content" на верхнем уровне
        }
        if (path.isEmpty()) {
            path = p["file_path"].toString();  // Поддержка "file_path" в params
        }
        if (content.isEmpty()) {
            content = p["file_content"].toString().toUtf8();  // Поддержка "file_content" вместо "content"
        }
        
        return new CreateFileCommand(path, content, description);
    }

    if (cmd == "REPLACE_IN_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString path = p["path"].toString();
        QByteArray content = p["content"].toString().toUtf8();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (path.isEmpty()) {
            path = obj["file_path"].toString();
        }
        if (content.isEmpty()) {
            content = obj["content"].toString().toUtf8();
        }
        if (path.isEmpty()) {
            path = p["file_path"].toString();
        }
        if (content.isEmpty()) {
            content = p["file_content"].toString().toUtf8();
        }
        
        return new ReplaceInFileCommand(path, content, description);
    }

    if (cmd == "DELETE_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString path = p["path"].toString();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (path.isEmpty()) {
            path = obj["file_path"].toString();
        }
        if (path.isEmpty()) {
            path = p["file_path"].toString();
        }
        
        return new DeleteFileCommand(path, description);
    }

    if (cmd == "RENAME_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString oldPath = p["oldPath"].toString();
        QString newPath = p["newPath"].toString();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (oldPath.isEmpty()) {
            oldPath = obj["old_path"].toString();
        }
        if (newPath.isEmpty()) {
            newPath = obj["new_path"].toString();
        }
        if (oldPath.isEmpty()) {
            oldPath = p["old_path"].toString();
        }
        if (newPath.isEmpty()) {
            newPath = p["new_path"].toString();
        }
        
        return new RenameFileCommand(oldPath, newPath, description);
    }

    if (cmd == "EDIT_IN_RANGE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString path = p["path"].toString();
        int startLine = p["startLine"].toInt();
        int endLine = p["endLine"].toInt();
        QByteArray content = p.value("content").toString().toUtf8();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (path.isEmpty()) {
            path = obj["file_path"].toString();
        }
        if (path.isEmpty()) {
            path = p["file_path"].toString();
        }
        if (startLine == 0) {
            startLine = obj["start_line"].toInt();
        }
        if (startLine == 0) {
            startLine = p["start_line"].toInt();
        }
        if (endLine == 0) {
            endLine = obj["end_line"].toInt();
        }
        if (endLine == 0) {
            endLine = p["end_line"].toInt();
        }
        if (content.isEmpty()) {
            content = obj["content"].toString().toUtf8();
        }
        if (content.isEmpty()) {
            content = p["file_content"].toString().toUtf8();
        }
        
        QJsonArray linesArray = p.value("replacementLines").toArray();
        if (linesArray.isEmpty() && p.contains("lines"))
            linesArray = p.value("lines").toArray();
        
        return new EditInRangeCommand(path, startLine, endLine, content, linesArray, description);
    }

    if (cmd == "WRITE_DESCRIPTION") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString sourceFilePath = p["sourceFilePath"].toString();
        QString description = p["description"].toString();
        
        // Поддержка альтернативного формата
        if (sourceFilePath.isEmpty()) {
            sourceFilePath = obj["file_path"].toString();
        }
        if (sourceFilePath.isEmpty()) {
            sourceFilePath = p["file_path"].toString();
        }
        
        if (description.isEmpty()) {
            description = obj["description_text"].toString();
        }
        if (description.isEmpty()) {
            description = p["description_text"].toString();
        }
        
        return new WriteDescriptionCommand(sourceFilePath, description);
    }

    // +++ в CommandRegistry::create()
    if (cmd == "OPEN_FILE_REGION") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString path = p["path"].toString();
        QString region = p.value("region").toString();
        int startLine = p.value("startLine").toInt(-1);
        int endLine = p.value("endLine").toInt(-1);
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (path.isEmpty()) {
            path = obj["file_path"].toString();
        }
        if (path.isEmpty()) {
            path = p["file_path"].toString();
        }
        if (region.isEmpty()) {
            region = obj["region"].toString();
        }
        if (region.isEmpty()) {
            region = p["region_text"].toString();
        }
        
        return new OpenFileRegionCommand(path, region, startLine, endLine, description);
    }

    if (cmd == "OPEN_FILE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString path = p["path"].toString();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (path.isEmpty()) {
            path = obj["file_path"].toString();
        }
        if (path.isEmpty()) {
            path = p["file_path"].toString();
        }
        
        return new OpenFileContextCommand(path, description);
    }

    if (cmd == "ASK_USER") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString question = p.value("question").toString();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (question.isEmpty()) {
            question = obj["question"].toString();
        }
        if (question.isEmpty()) {
            question = p["question_text"].toString();
        }
        
        return new AskUserCommand(question, description);
    }

    // +++ в CommandRegistry::create()
    if (cmd == "RUN_PYTHON") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString scriptPath = p.value("scriptPath").toString();
        if (scriptPath.isEmpty())
            scriptPath = p.value("script_path").toString();
        if (scriptPath.isEmpty())
            scriptPath = p.value("path").toString();
        
        // Поддержка альтернативного формата
        if (scriptPath.isEmpty()) {
            scriptPath = obj["script_path"].toString();
        }
        if (scriptPath.isEmpty()) {
            scriptPath = obj["file_path"].toString();
        }

        QString codeStr = p.value("code").toString();
        if (codeStr.isEmpty())
            codeStr = p.value("inlineCode").toString();
        if (codeStr.isEmpty())
            codeStr = p.value("inline_code").toString();
        if (codeStr.isEmpty())
            codeStr = p.value("python").toString();
            
        // Поддержка альтернативного формата
        if (codeStr.isEmpty()) {
            codeStr = obj["code"].toString();
        }
        if (codeStr.isEmpty()) {
            codeStr = p["python_code"].toString();
        }

        return new RunPythonCommand(scriptPath, codeStr.toUtf8(), obj["description"].toString());
    }

    if (cmd == "RUN_SHELL") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString command = p.value("command").toString();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (command.isEmpty()) {
            command = obj["command_text"].toString();
        }
        if (command.isEmpty()) {
            command = p["command_text"].toString();
        }
        
        return new RunShellCommand(command, description);
    }

    if (cmd == "SEARCH_WEB") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString query = p.value("query").toString();
        int maxResults = p.value("maxResults").toInt(5);
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (query.isEmpty()) {
            query = obj["query"].toString();
        }
        if (query.isEmpty()) {
            query = p["search_query"].toString();
        }
        
        return new SearchWebCommand(query, maxResults, description);
    }

    if (cmd == "SEARCH_PROJECT") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString query = p.value("query").toString();
        int maxMatches = p.value("maxMatches").toInt(10);
        bool caseSensitive = p.value("caseSensitive").toBool(false);
        QString description = obj["description"].toString();
        
        QStringList includes;
        QJsonArray arr = p.value("include").toArray();
        if (arr.isEmpty()) {
            // Поддержка альтернативного формата для include
            arr = obj.value("include").toArray();
        }
        for (const QJsonValue& v : arr)
            includes << v.toString();
        
        // Поддержка альтернативного формата для query
        if (query.isEmpty()) {
            query = obj["query"].toString();
        }
        if (query.isEmpty()) {
            query = p["search_query"].toString();
        }
        
        return new SearchProjectCommand(query, maxMatches, caseSensitive, includes, description);
    }

    if (cmd == "SEARCH_PROJECT_CASE_INSENSITIVE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString query = p.value("query").toString();
        int maxMatches = p.value("maxMatches").toInt(10);
        QString description = obj["description"].toString();
        
        QStringList includes;
        QJsonArray arr = p.value("include").toArray();
        if (arr.isEmpty()) {
            // Поддержка альтернативного формата для include
            arr = obj.value("include").toArray();
        }
        for (const QJsonValue& v : arr)
            includes << v.toString();
        
        // Поддержка альтернативного формата для query
        if (query.isEmpty()) {
            query = obj["query"].toString();
        }
        if (query.isEmpty()) {
            query = p["search_query"].toString();
        }
        
        return new SearchProjectCaseInsensitiveCommand(query, maxMatches, includes, description);
    }

    if (cmd == "SHOW_RESULT") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString content = p.value("content").toString();
        QString title = p.value("title").toString();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (content.isEmpty()) {
            content = obj["content"].toString();
        }
        if (content.isEmpty()) {
            content = p["result_content"].toString();
        }
        if (title.isEmpty()) {
            title = obj["title"].toString();
        }
        if (title.isEmpty()) {
            title = p["result_title"].toString();
        }
        
        return new ShowResultCommand(content, title, description);
    }

    if (cmd == "CALL_SEQUENCE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString sequenceName = p["sequenceName"].toString();
        QString description = obj["description"].toString();
        bool needSummary = true;
        
        // Поддержка альтернативного формата
        if (sequenceName.isEmpty()) {
            sequenceName = obj["sequence_name"].toString();
        }
        if (sequenceName.isEmpty()) {
            sequenceName = p["sequence_name"].toString();
        }
        
        if (p.contains("summarize")) {
            needSummary = p.value("summarize").toBool();
        } else if (obj.contains("summarize")) {
            needSummary = obj.value("summarize").toBool();
        }
        
        return new CallSequenceCommand(sequenceName, needSummary, description);
    }

    if (cmd == "OPEN_SEQUENCE") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString sequenceName = p["sequenceName"].toString();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (sequenceName.isEmpty()) {
            sequenceName = obj["sequence_name"].toString();
        }
        if (sequenceName.isEmpty()) {
            sequenceName = p["sequence_name"].toString();
        }
        
        return new OpenSequenceCommand(sequenceName, description);
    }

    if (cmd == "LIST_FILES_IN_FOLDER") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString folderPath = p["folderPath"].toString();
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (folderPath.isEmpty()) {
            folderPath = obj["folder_path"].toString();
        }
        if (folderPath.isEmpty()) {
            folderPath = p["folder_path"].toString();
        }
        
        return new ListFilesInFolderCommand(folderPath, description);
    }

    if (cmd == "REASONING") {
        const QJsonObject p = paramsObject(obj.value("params"));
        QString reasoning = p.value("reasoning").toString();
        QString nextCommand = p.value("nextCommand").toString();
        QJsonObject nextCommandParams = p.value("nextCommandParams").toObject();
        QString description = obj["description"].toString();

        // Поддержка альтернативного формата
        if (reasoning.isEmpty()) {
            reasoning = obj["reasoning"].toString();
        }
        if (reasoning.isEmpty()) {
            reasoning = p["thought_process"].toString();
        }
        if (reasoning.isEmpty()) {
            reasoning = p["reasoning_text"].toString();
        }
        if (nextCommand.isEmpty()) {
            nextCommand = obj["next_command"].toString();
        }
        if (nextCommand.isEmpty()) {
            nextCommand = p["next_command"].toString();
        }

        return new ReasoningCommand(reasoning, nextCommand, nextCommandParams, description);
    }

    /* Неизвестная команда */
    return nullptr;
}
