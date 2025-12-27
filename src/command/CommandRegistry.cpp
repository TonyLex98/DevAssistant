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
#include "CopyFileCommand.h"
#include "CreateDirCommand.h"
#include "DeleteDirCommand.h"
#include "AppendToFileCommand.h"
#include "InsertAtLineCommand.h"
#include "ApplyPatchCommand.h"
#include "GitStatusCommand.h"
#include "GitDiffCommand.h"
#include "GitRestoreFileCommand.h"
#include "UndoLastChangeCommand.h"
#include "ContextShowCommand.h"
#include "ContextClearCommand.h"
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

static QJsonObject normalizedParams(const QJsonObject& obj)
{
    QJsonObject p = paramsObject(obj.value("params"));
    if (!p.isEmpty())
        return p;

    // Fallback: treat any extra top-level fields as params.
    QJsonObject out;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString key = it.key();
        if (key == "command" || key == "action" || key == "description")
            continue;
        out.insert(key, it.value());
    }
    return out;
}

static QJsonObject objectFromValue(const QJsonValue& value)
{
    if (value.isObject())
        return value.toObject();
    if (!value.isString())
        return QJsonObject();
    return parseJsonObject(value.toString().trimmed().toUtf8());
}

static QString firstNonEmptyString(const QJsonObject& obj, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString v = obj.value(key).toString().trimmed();
        if (!v.isEmpty())
            return v;
    }
    return QString();
}

static int firstNonZeroInt(const QJsonObject& obj, const QStringList& keys, int fallback = 0)
{
    for (const QString& key : keys) {
        const int v = obj.value(key).toInt();
        if (v != 0)
            return v;
    }
    return fallback;
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
        new ReasoningCommand(QString(), QString(), QJsonObject()),
        new CopyFileCommand(QString(), QString(), false),
        new CreateDirCommand(QString()),
        new DeleteDirCommand(QString(), false),
        new AppendToFileCommand(QString(), QByteArray(), true, false),
        new InsertAtLineCommand(QString(), 1, QByteArray()),
        new ApplyPatchCommand(QByteArray(), 0),
        new GitStatusCommand(true),
        new GitDiffCommand(false),
        new GitRestoreFileCommand(QString(), false),
        new UndoLastChangeCommand(),
        new ContextShowCommand(8000),
        new ContextClearCommand(QStringLiteral("active"))
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
        const QJsonObject p = normalizedParams(obj);
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
        const QJsonObject p = normalizedParams(obj);
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
        const QJsonObject p = normalizedParams(obj);
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
        const QJsonObject p = normalizedParams(obj);
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
        const QJsonObject p = normalizedParams(obj);
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
        const QJsonObject p = normalizedParams(obj);
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
        const QJsonObject p = normalizedParams(obj);
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
        const QJsonObject p = normalizedParams(obj);
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
        const QJsonObject p = normalizedParams(obj);
        QString question = firstNonEmptyString(p, {"question", "question_text"});
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
        const QJsonObject p = normalizedParams(obj);
        QString scriptPath = firstNonEmptyString(p, {"scriptPath", "script_path", "path", "file_path"});
        
        // Поддержка альтернативного формата
        if (scriptPath.isEmpty()) {
            scriptPath = obj["script_path"].toString();
        }
        if (scriptPath.isEmpty()) {
            scriptPath = obj["file_path"].toString();
        }

        QString codeStr = firstNonEmptyString(p, {"code", "inlineCode", "inline_code", "python", "python_code", "content"});
            
        // Поддержка альтернативного формата
        if (codeStr.isEmpty()) {
            codeStr = obj["code"].toString();
        }
        if (codeStr.isEmpty()) {
            codeStr = p["python_code"].toString();
        }

        const QString cwd = firstNonEmptyString(p, {"cwd", "workdir", "workingDirectory"});
        return new RunPythonCommand(scriptPath, codeStr.toUtf8(), cwd, obj["description"].toString());
    }

    if (cmd == "RUN_SHELL") {
        const QJsonObject p = normalizedParams(obj);
        QString command = firstNonEmptyString(p, {"command", "cmd", "command_text"});
        QString cwd = firstNonEmptyString(p, {"cwd", "workdir", "workingDirectory"});
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (command.isEmpty()) {
            command = obj["command_text"].toString();
        }
        if (command.isEmpty()) {
            command = p["command_text"].toString();
        }
        
        return new RunShellCommand(command, cwd, description);
    }

    if (cmd == "SEARCH_WEB") {
        const QJsonObject p = normalizedParams(obj);
        QString query = firstNonEmptyString(p, {"query", "search_query"});
        int maxResults = firstNonZeroInt(p, {"maxResults", "max_results"}, 5);
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
        const QJsonObject p = normalizedParams(obj);
        QString query = firstNonEmptyString(p, {"query", "search_query"});
        int maxMatches = firstNonZeroInt(p, {"maxMatches", "max_matches"}, 10);
        bool caseSensitive = p.value("caseSensitive").toBool(p.value("case_sensitive").toBool(false));
        QString description = obj["description"].toString();
        
        QStringList includes;
        QJsonArray arr = p.value("include").toArray();
        if (arr.isEmpty())
            arr = p.value("includeGlobs").toArray();
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
        
        bool useRegex = p.value("useRegex").toBool(p.value("use_regex").toBool(false));
        int contextLines = p.value("contextLines").toInt(p.value("context_lines").toInt(0));
        QStringList includePaths = QStringList();
        QStringList excludePaths = QStringList();

        // Читаем includePaths если они есть
        QJsonArray includePathsArr = p.value("includePaths").toArray();
        for (const QJsonValue& v : includePathsArr) {
            includePaths << v.toString();
        }

        // Читаем excludePaths если они есть
        QJsonArray excludePathsArr = p.value("excludePaths").toArray();
        for (const QJsonValue& v : excludePathsArr) {
            excludePaths << v.toString();
        }

        return new SearchProjectCommand(query, maxMatches, caseSensitive, includes, useRegex, contextLines, includePaths, excludePaths, description);
    }

    if (cmd == "SEARCH_PROJECT_CASE_INSENSITIVE") {
        const QJsonObject p = normalizedParams(obj);
        QString query = firstNonEmptyString(p, {"query", "search_query"});
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
        const QJsonObject p = normalizedParams(obj);
        QString content = firstNonEmptyString(p, {"content", "text", "result", "message"});
        QString title = firstNonEmptyString(p, {"title", "result_title"});
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
        const QJsonObject p = normalizedParams(obj);
        QString sequenceName = firstNonEmptyString(p, {"sequenceName", "sequence", "name", "sequence_name"});
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
        const QJsonObject p = normalizedParams(obj);
        QString sequenceName = firstNonEmptyString(p, {"sequenceName", "sequence", "name", "sequence_name"});
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
        const QJsonObject p = normalizedParams(obj);
        QString folderPath = firstNonEmptyString(p, {"folderPath", "path", "folder_path"});
        const QString pattern = firstNonEmptyString(p, {"pattern", "filter", "glob"});
        QString description = obj["description"].toString();
        
        // Поддержка альтернативного формата
        if (folderPath.isEmpty()) {
            folderPath = obj["folder_path"].toString();
        }
        if (folderPath.isEmpty()) {
            folderPath = p["folder_path"].toString();
        }
        
        return new ListFilesInFolderCommand(folderPath, pattern, description);
    }

    if (cmd == "REASONING") {
        const QJsonObject p = normalizedParams(obj);
        QString reasoning = firstNonEmptyString(p, {"reasoning", "thoughts"});
        QString nextCommand = firstNonEmptyString(p, {"nextCommand", "next_command"});
        QJsonObject nextCommandParams = objectFromValue(p.value("nextCommandParams"));
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

    if (cmd == "COPY_FILE") {
        const QJsonObject p = normalizedParams(obj);
        const QString from = firstNonEmptyString(p, {"from", "src", "source", "sourcePath", "source_path"});
        const QString to = firstNonEmptyString(p, {"to", "dst", "dest", "destination", "destinationPath", "dest_path"});
        const bool overwrite = p.value("overwrite").toBool(obj.value("overwrite").toBool(false));
        const QString description = obj["description"].toString();
        return new CopyFileCommand(from, to, overwrite, description);
    }

    if (cmd == "CREATE_DIR") {
        const QJsonObject p = normalizedParams(obj);
        const QString path = firstNonEmptyString(p, {"path", "dir", "folder", "directory"});
        const QString description = obj["description"].toString();
        return new CreateDirCommand(path, description);
    }

    if (cmd == "DELETE_DIR") {
        const QJsonObject p = normalizedParams(obj);
        const QString path = firstNonEmptyString(p, {"path", "dir", "folder", "directory"});
        const bool recursive = p.value("recursive").toBool(obj.value("recursive").toBool(false));
        const QString description = obj["description"].toString();
        return new DeleteDirCommand(path, recursive, description);
    }

    if (cmd == "APPEND_TO_FILE") {
        const QJsonObject p = normalizedParams(obj);
        const QString path = firstNonEmptyString(p, {"path", "file", "filePath", "file_path"});
        const QString content = firstNonEmptyString(p, {"content", "text", "data"});
        const bool createIfMissing = p.value("createIfMissing").toBool(obj.value("createIfMissing").toBool(true));
        const bool ensureNewline = p.value("ensureNewline").toBool(obj.value("ensureNewline").toBool(false));
        const QString description = obj["description"].toString();
        return new AppendToFileCommand(path, content.toUtf8(), createIfMissing, ensureNewline, description);
    }

    if (cmd == "INSERT_AT_LINE") {
        const QJsonObject p = normalizedParams(obj);
        const QString path = firstNonEmptyString(p, {"path", "file", "filePath", "file_path"});
        const int line = firstNonZeroInt(p, {"line", "lineNumber", "line_number"}, obj.value("line").toInt());
        const QString content = firstNonEmptyString(p, {"content", "text"});
        const QString description = obj["description"].toString();
        return new InsertAtLineCommand(path, line, content.toUtf8(), description);
    }

    if (cmd == "APPLY_PATCH") {
        const QJsonObject p = normalizedParams(obj);
        const QString patch = firstNonEmptyString(p, {"patch", "diff"});
        const int strip = p.value("strip").toInt(obj.value("strip").toInt(0));
        const QString description = obj["description"].toString();
        return new ApplyPatchCommand(patch.toUtf8(), strip, description);
    }

    if (cmd == "GIT_STATUS") {
        const QJsonObject p = normalizedParams(obj);
        const bool porcelain = p.value("porcelain").toBool(obj.value("porcelain").toBool(true));
        const QString path = firstNonEmptyString(p, {"path", "file"});
        const QString description = obj["description"].toString();
        return new GitStatusCommand(porcelain, path, description);
    }

    if (cmd == "GIT_DIFF") {
        const QJsonObject p = normalizedParams(obj);
        const bool staged = p.value("staged").toBool(obj.value("staged").toBool(false));
        const int contextLines = p.value("contextLines").toInt(obj.value("contextLines").toInt(-1));
        const QString path = firstNonEmptyString(p, {"path", "file"});
        const QString description = obj["description"].toString();
        return new GitDiffCommand(staged, contextLines, path, description);
    }

    if (cmd == "GIT_RESTORE_FILE") {
        const QJsonObject p = normalizedParams(obj);
        const QString path = firstNonEmptyString(p, {"path", "file"});
        const bool staged = p.value("staged").toBool(obj.value("staged").toBool(false));
        const bool worktree = p.value("worktree").toBool(obj.value("worktree").toBool(true));
        const QString description = obj["description"].toString();
        return new GitRestoreFileCommand(path, staged, worktree, description);
    }

    if (cmd == "UNDO_LAST_CHANGE") {
        const QString description = obj["description"].toString();
        return new UndoLastChangeCommand(description);
    }

    if (cmd == "CONTEXT_SHOW") {
        const QJsonObject p = normalizedParams(obj);
        const int maxChars = p.value("maxChars").toInt(obj.value("maxChars").toInt(8000));
        const QString description = obj["description"].toString();
        return new ContextShowCommand(maxChars, description);
    }

    if (cmd == "CONTEXT_CLEAR") {
        const QJsonObject p = normalizedParams(obj);
        const QString scope = firstNonEmptyString(p, {"scope"});
        const QString description = obj["description"].toString();
        return new ContextClearCommand(scope, description);
    }

    /* Неизвестная команда */
    return nullptr;
}
