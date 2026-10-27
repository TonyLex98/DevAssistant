// CommandRegistry.cpp
// Macro-based command registration to replace the monolithic if-else chain.
// Each command is registered via REGISTER_COMMAND() with a factory lambda that
// extracts parameters from the normalized JSON and returns a Command*.
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
#include "OpenAppCommand.h"
#include "OpenUrlCommand.h"
#include "ClipboardSetCommand.h"
#include "ClipboardGetCommand.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QRegularExpression>
#include <functional>

/* ------------------------------------------------------------------ */
/*  Helpers (unchanged)                                                */
/* ------------------------------------------------------------------ */

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

static QString normalizeKeyToken(const QString& in)
{
    QString out = in.trimmed().toLower();
    out.replace(QRegularExpression("[^a-z0-9]+"), "_");
    out.replace(QRegularExpression("_+"), "_");
    if (out.startsWith('_'))
        out.remove(0, 1);
    if (out.endsWith('_'))
        out.chop(1);
    return out;
}

static QString canonicalCommandName(const QString& raw)
{
    const QString token = normalizeKeyToken(raw);
    if (token.isEmpty())
        return QString();

    static const QHash<QString, QString> kAliases = {
        {"create_file", "CREATE_FILE"},
        {"new_file", "CREATE_FILE"},
        {"write_file", "CREATE_FILE"},
        {"make_file", "CREATE_FILE"},
        {"replace_in_file", "REPLACE_IN_FILE"},
        {"overwrite_file", "REPLACE_IN_FILE"},
        {"replace_file", "REPLACE_IN_FILE"},
        {"edit_in_range", "EDIT_IN_RANGE"},
        {"edit_range", "EDIT_IN_RANGE"},
        {"patch_range", "EDIT_IN_RANGE"},
        {"delete_file", "DELETE_FILE"},
        {"remove_file", "DELETE_FILE"},
        {"rename_file", "RENAME_FILE"},
        {"move_file", "RENAME_FILE"},
        {"open_file", "OPEN_FILE"},
        {"read_file", "OPEN_FILE"},
        {"show_file", "OPEN_FILE"},
        {"open", "OPEN_FILE"},
        {"open_file_region", "OPEN_FILE_REGION"},
        {"read_region", "OPEN_FILE_REGION"},
        {"show_region", "OPEN_FILE_REGION"},
        {"ask_user", "ASK_USER"},
        {"ask", "ASK_USER"},
        {"question", "ASK_USER"},
        {"run_python", "RUN_PYTHON"},
        {"python", "RUN_PYTHON"},
        {"run_py", "RUN_PYTHON"},
        {"run_shell", "RUN_SHELL"},
        {"shell", "RUN_SHELL"},
        {"cmd", "RUN_SHELL"},
        {"terminal", "RUN_SHELL"},
        {"search_web", "SEARCH_WEB"},
        {"web_search", "SEARCH_WEB"},
        {"internet_search", "SEARCH_WEB"},
        {"search_project", "SEARCH_PROJECT"},
        {"grep_project", "SEARCH_PROJECT"},
        {"grep", "SEARCH_PROJECT"},
        {"search_code", "SEARCH_PROJECT"},
        {"search_project_case_insensitive", "SEARCH_PROJECT_CASE_INSENSITIVE"},
        {"show_result", "SHOW_RESULT"},
        {"result", "SHOW_RESULT"},
        {"final_answer", "SHOW_RESULT"},
        {"call_sequence", "CALL_SEQUENCE"},
        {"run_sequence", "CALL_SEQUENCE"},
        {"next_sequence", "CALL_SEQUENCE"},
        {"open_sequence", "OPEN_SEQUENCE"},
        {"list_files_in_folder", "LIST_FILES_IN_FOLDER"},
        {"list_files", "LIST_FILES_IN_FOLDER"},
        {"list_dir", "LIST_FILES_IN_FOLDER"},
        {"ls", "LIST_FILES_IN_FOLDER"},
        {"reasoning", "REASONING"},
        {"think", "REASONING"},
        {"copy_file", "COPY_FILE"},
        {"cp_file", "COPY_FILE"},
        {"create_dir", "CREATE_DIR"},
        {"mkdir", "CREATE_DIR"},
        {"delete_dir", "DELETE_DIR"},
        {"remove_dir", "DELETE_DIR"},
        {"rmdir", "DELETE_DIR"},
        {"append_to_file", "APPEND_TO_FILE"},
        {"append_file", "APPEND_TO_FILE"},
        {"insert_at_line", "INSERT_AT_LINE"},
        {"insert_line", "INSERT_AT_LINE"},
        {"apply_patch", "APPLY_PATCH"},
        {"patch", "APPLY_PATCH"},
        {"git_status", "GIT_STATUS"},
        {"status", "GIT_STATUS"},
        {"git_diff", "GIT_DIFF"},
        {"diff", "GIT_DIFF"},
        {"git_restore_file", "GIT_RESTORE_FILE"},
        {"restore_file", "GIT_RESTORE_FILE"},
        {"undo_last_change", "UNDO_LAST_CHANGE"},
        {"undo", "UNDO_LAST_CHANGE"},
        {"context_show", "CONTEXT_SHOW"},
        {"show_context", "CONTEXT_SHOW"},
        {"context_clear", "CONTEXT_CLEAR"},
        {"clear_context", "CONTEXT_CLEAR"},
        {"write_description", "WRITE_DESCRIPTION"},
        {"describe_file", "WRITE_DESCRIPTION"},
        {"open_app", "OPEN_APP"},
        {"launch_app", "OPEN_APP"},
        {"start_app", "OPEN_APP"},
        {"open_url", "OPEN_URL"},
        {"open_link", "OPEN_URL"},
        {"browser_open", "OPEN_URL"},
        {"clipboard_set", "CLIPBOARD_SET"},
        {"set_clipboard", "CLIPBOARD_SET"},
        {"copy_to_clipboard", "CLIPBOARD_SET"},
        {"clipboard_get", "CLIPBOARD_GET"},
        {"get_clipboard", "CLIPBOARD_GET"},
        {"read_clipboard", "CLIPBOARD_GET"}
    };

    auto it = kAliases.find(token);
    if (it != kAliases.end())
        return it.value();

    return token.toUpper();
}

static void mapAliasValue(QJsonObject& p, const QString& canonical, const QStringList& aliases)
{
    if (p.contains(canonical) && !p.value(canonical).isNull())
        return;

    for (const QString& alias : aliases) {
        for (auto it = p.constBegin(); it != p.constEnd(); ++it) {
            if (normalizeKeyToken(it.key()) == normalizeKeyToken(alias)) {
                p.insert(canonical, it.value());
                return;
            }
        }
    }
}

static QJsonObject normalizeParamAliases(QJsonObject p)
{
    mapAliasValue(p, "path", {"file_path", "file", "filepath", "filePath", "target_path", "target"});
    mapAliasValue(p, "content", {"text", "data", "file_content", "value", "body"});
    mapAliasValue(p, "query", {"search_query", "search", "pattern", "needle", "keyword"});
    mapAliasValue(p, "command", {"cmd", "shell", "shell_command", "command_text"});
    mapAliasValue(p, "cwd", {"workdir", "working_directory", "workingDirectory", "dir"});
    mapAliasValue(p, "sequenceName", {"sequence_name", "sequence", "name"});
    mapAliasValue(p, "startLine", {"start_line", "start", "line_start", "from_line"});
    mapAliasValue(p, "endLine", {"end_line", "end", "line_end", "to_line"});
    mapAliasValue(p, "oldPath", {"old_path", "source_path", "source", "from_path", "src", "from"});
    mapAliasValue(p, "newPath", {"new_path", "destination_path", "destination", "dest", "dst", "to_path"});
    mapAliasValue(p, "line", {"line_number", "lineNumber", "row"});
    mapAliasValue(p, "folderPath", {"folder_path", "folder", "directory", "dir", "path"});
    mapAliasValue(p, "scriptPath", {"script_path", "script", "py", "python_file"});
    mapAliasValue(p, "question", {"question_text", "prompt", "ask"});
    mapAliasValue(p, "title", {"result_title", "heading"});
    mapAliasValue(p, "app", {"application", "program", "exe", "path"});
    mapAliasValue(p, "url", {"link", "uri", "address"});
    mapAliasValue(p, "text", {"content", "value", "clipboard_text"});
    mapAliasValue(p, "maxChars", {"max_chars", "limit", "count"});
    mapAliasValue(p, "maxResults", {"max_results", "limit", "count"});
    mapAliasValue(p, "maxMatches", {"max_matches", "limit", "count"});
    mapAliasValue(p, "waitMs", {"wait_ms", "timeoutMs", "timeout_ms", "timeout", "max_wait_ms"});
    mapAliasValue(p, "caseSensitive", {"case_sensitive"});
    mapAliasValue(p, "useRegex", {"use_regex", "regex"});
    mapAliasValue(p, "contextLines", {"context_lines", "context"});
    mapAliasValue(p, "from", {"source", "src", "source_path"});
    mapAliasValue(p, "to", {"destination", "dest", "dst", "target_path"});
    mapAliasValue(p, "filePath", {"file_path", "path"});

    if (p.contains("include_globs") && !p.contains("include"))
        p.insert("include", p.value("include_globs"));

    return p;
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
    if (p.isEmpty())
        p = paramsObject(obj.value("arguments"));
    if (p.isEmpty())
        p = paramsObject(obj.value("args"));
    if (!p.isEmpty())
        return normalizeParamAliases(p);

    // Fallback: treat any extra top-level fields as params.
    QJsonObject out;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString keyNorm = normalizeKeyToken(it.key());
        if (keyNorm == "command" || keyNorm == "action" || keyNorm == "description" ||
            keyNorm == "type" || keyNorm == "tool")
            continue;
        out.insert(it.key(), it.value());
    }
    return normalizeParamAliases(out);
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
        QString v = obj.value(key).toString().trimmed();
        if (v.isEmpty()) {
            const QString normalizedWanted = normalizeKeyToken(key);
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                if (normalizeKeyToken(it.key()) == normalizedWanted) {
                    v = it.value().toString().trimmed();
                    if (!v.isEmpty())
                        break;
                }
            }
        }
        if (!v.isEmpty())
            return v;
    }
    return QString();
}

static int firstNonZeroInt(const QJsonObject& obj, const QStringList& keys, int fallback = 0)
{
    for (const QString& key : keys) {
        int v = obj.value(key).toInt();
        if (v == 0) {
            const QString normalizedWanted = normalizeKeyToken(key);
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                if (normalizeKeyToken(it.key()) == normalizedWanted) {
                    v = it.value().toInt();
                    if (v != 0)
                        break;
                }
            }
        }
        if (v != 0)
            return v;
    }
    return fallback;
}

static QString resolvePath(const QJsonObject& p, const QJsonObject& obj)
{
    QString path = p["path"].toString();
    if (path.isEmpty()) path = obj["file_path"].toString();
    if (path.isEmpty()) path = p["file_path"].toString();
    return path;
}

static QByteArray resolveContent(const QJsonObject& p, const QJsonObject& obj)
{
    QByteArray content = p["content"].toString().toUtf8();
    if (content.isEmpty()) content = obj["content"].toString().toUtf8();
    if (content.isEmpty()) content = p["file_content"].toString().toUtf8();
    return content;
}

/* ------------------------------------------------------------------ */
/*  Factory type & registry                                            */
/* ------------------------------------------------------------------ */

using FactoryFn = std::function<Command*(const QJsonObject&)>;

static QHash<QString, FactoryFn>& registry()
{
    static QHash<QString, FactoryFn> s_registry;
    static bool s_initialized = false;
    if (!s_initialized) {
        s_initialized = true;

#define REGISTER_COMMAND(NAME, BODY) \
    registry().insert(#NAME, (BODY))

        /* ---- File commands ---- */
        REGISTER_COMMAND(CREATE_FILE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString path = resolvePath(p, obj);
            QByteArray content = resolveContent(p, obj);
            QString description = obj["description"].toString();
            return new CreateFileCommand(path, content, description);
        });

        REGISTER_COMMAND(REPLACE_IN_FILE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString path = resolvePath(p, obj);
            QByteArray content = resolveContent(p, obj);
            QString description = obj["description"].toString();
            return new ReplaceInFileCommand(path, content, description);
        });

        REGISTER_COMMAND(DELETE_FILE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString path = resolvePath(p, obj);
            QString description = obj["description"].toString();
            return new DeleteFileCommand(path, description);
        });

        REGISTER_COMMAND(RENAME_FILE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString oldPath = p["oldPath"].toString();
            QString newPath = p["newPath"].toString();
            if (oldPath.isEmpty()) oldPath = obj["old_path"].toString();
            if (newPath.isEmpty()) newPath = obj["new_path"].toString();
            if (oldPath.isEmpty()) oldPath = p["old_path"].toString();
            if (newPath.isEmpty()) newPath = p["new_path"].toString();
            QString description = obj["description"].toString();
            return new RenameFileCommand(oldPath, newPath, description);
        });

        REGISTER_COMMAND(EDIT_IN_RANGE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString path = resolvePath(p, obj);
            int startLine = p["startLine"].toInt();
            int endLine = p["endLine"].toInt();
            QByteArray content = p.value("content").toString().toUtf8();
            if (content.isEmpty()) content = obj["content"].toString().toUtf8();
            if (content.isEmpty()) content = p["file_content"].toString().toUtf8();
            if (startLine == 0) startLine = obj["start_line"].toInt();
            if (startLine == 0) startLine = p["start_line"].toInt();
            if (endLine == 0) endLine = obj["end_line"].toInt();
            if (endLine == 0) endLine = p["end_line"].toInt();
            QJsonArray linesArray = p.value("replacementLines").toArray();
            if (linesArray.isEmpty() && p.contains("lines"))
                linesArray = p.value("lines").toArray();
            QString description = obj["description"].toString();
            return new EditInRangeCommand(path, startLine, endLine, content, linesArray, description);
        });

        REGISTER_COMMAND(WRITE_DESCRIPTION, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString sourceFilePath = p["sourceFilePath"].toString();
            if (sourceFilePath.isEmpty()) sourceFilePath = resolvePath(p, obj);
            QString description = p["description"].toString();
            if (description.isEmpty()) description = obj["description"].toString();
            if (description.isEmpty()) description = p["description_text"].toString();
            return new WriteDescriptionCommand(sourceFilePath, description);
        });

        REGISTER_COMMAND(OPEN_FILE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString path = resolvePath(p, obj);
            QString description = obj["description"].toString();
            return new OpenFileContextCommand(path, description);
        });

        REGISTER_COMMAND(OPEN_FILE_REGION, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString path = resolvePath(p, obj);
            QString region = p.value("region").toString();
            if (region.isEmpty()) region = p["region_text"].toString();
            int startLine = p.value("startLine").toInt(-1);
            int endLine = p.value("endLine").toInt(-1);
            QString description = obj["description"].toString();
            return new OpenFileRegionCommand(path, region, startLine, endLine, description);
        });

        /* ---- User interaction ---- */
        REGISTER_COMMAND(ASK_USER, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString question = firstNonEmptyString(p, {"question", "question_text"});
            if (question.isEmpty()) question = obj["question"].toString();
            QString description = obj["description"].toString();
            return new AskUserCommand(question, description);
        });

        /* ---- Script / shell ---- */
        REGISTER_COMMAND(RUN_PYTHON, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString scriptPath = firstNonEmptyString(p, {"scriptPath", "script_path", "path", "file_path"});
            if (scriptPath.isEmpty()) scriptPath = obj["script_path"].toString();
            if (scriptPath.isEmpty()) scriptPath = obj["file_path"].toString();
            QString codeStr = firstNonEmptyString(p, {"code", "inlineCode", "inline_code", "python", "python_code", "content"});
            if (codeStr.isEmpty()) codeStr = obj["code"].toString();
            if (codeStr.isEmpty()) codeStr = p["python_code"].toString();
            const QString cwd = firstNonEmptyString(p, {"cwd", "workdir", "workingDirectory"});
            const int timeoutMs = p.value("timeoutMs").toInt(obj.value("timeoutMs").toInt(120000));
            return new RunPythonCommand(scriptPath, codeStr.toUtf8(), cwd, timeoutMs, obj["description"].toString());
        });

        REGISTER_COMMAND(RUN_SHELL, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString command = firstNonEmptyString(p, {"command", "cmd", "command_text"});
            if (command.isEmpty()) command = obj["command_text"].toString();
            if (command.isEmpty()) command = p["command_text"].toString();
            QString cwd = firstNonEmptyString(p, {"cwd", "workdir", "workingDirectory"});
            const int timeoutMs = p.value("timeoutMs").toInt(obj.value("timeoutMs").toInt(120000));
            QString description = obj["description"].toString();
            return new RunShellCommand(command, cwd, timeoutMs, description);
        });

        /* ---- Search ---- */
        REGISTER_COMMAND(SEARCH_WEB, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString query = firstNonEmptyString(p, {"query", "search_query"});
            if (query.isEmpty()) query = obj["query"].toString();
            if (query.isEmpty()) query = p["search_query"].toString();
            int maxResults = firstNonZeroInt(p, {"maxResults", "max_results"}, 5);
            QString description = obj["description"].toString();
            return new SearchWebCommand(query, maxResults, description);
        });

        REGISTER_COMMAND(SEARCH_PROJECT, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString query = firstNonEmptyString(p, {"query", "search_query"});
            if (query.isEmpty()) query = obj["query"].toString();
            if (query.isEmpty()) query = p["search_query"].toString();
            int maxMatches = firstNonZeroInt(p, {"maxMatches", "max_matches"}, 10);
            bool caseSensitive = p.value("caseSensitive").toBool(p.value("case_sensitive").toBool(false));
            bool useRegex = p.value("useRegex").toBool(p.value("use_regex").toBool(false));
            int contextLines = p.value("contextLines").toInt(p.value("context_lines").toInt(0));
            QStringList includes;
            QJsonArray arr = p.value("include").toArray();
            if (arr.isEmpty()) arr = p.value("includeGlobs").toArray();
            if (arr.isEmpty()) arr = obj.value("include").toArray();
            for (const QJsonValue& v : arr) includes << v.toString();
            QStringList includePaths;
            QJsonArray includePathsArr = p.value("includePaths").toArray();
            for (const QJsonValue& v : includePathsArr) includePaths << v.toString();
            QStringList excludePaths;
            QJsonArray excludePathsArr = p.value("excludePaths").toArray();
            for (const QJsonValue& v : excludePathsArr) excludePaths << v.toString();
            QString description = obj["description"].toString();
            return new SearchProjectCommand(query, maxMatches, caseSensitive, includes, useRegex, contextLines, includePaths, excludePaths, description);
        });

        REGISTER_COMMAND(SEARCH_PROJECT_CASE_INSENSITIVE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString query = firstNonEmptyString(p, {"query", "search_query"});
            if (query.isEmpty()) query = obj["query"].toString();
            if (query.isEmpty()) query = p["search_query"].toString();
            int maxMatches = p.value("maxMatches").toInt(10);
            QStringList includes;
            QJsonArray arr = p.value("include").toArray();
            if (arr.isEmpty()) arr = obj.value("include").toArray();
            for (const QJsonValue& v : arr) includes << v.toString();
            QString description = obj["description"].toString();
            return new SearchProjectCaseInsensitiveCommand(query, maxMatches, includes, description);
        });

        /* ---- Result / output ---- */
        REGISTER_COMMAND(SHOW_RESULT, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString content = firstNonEmptyString(p, {"content", "text", "result", "message"});
            if (content.isEmpty()) content = obj["content"].toString();
            if (content.isEmpty()) content = p["result_content"].toString();
            QString title = firstNonEmptyString(p, {"title", "result_title"});
            if (title.isEmpty()) title = obj["title"].toString();
            if (title.isEmpty()) title = p["result_title"].toString();
            QString description = obj["description"].toString();
            return new ShowResultCommand(content, title, description);
        });

        /* ---- Sequences ---- */
        REGISTER_COMMAND(CALL_SEQUENCE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString sequenceName = firstNonEmptyString(p, {"sequenceName", "sequence", "name", "sequence_name"});
            if (sequenceName.isEmpty()) sequenceName = obj["sequence_name"].toString();
            if (sequenceName.isEmpty()) sequenceName = p["sequence_name"].toString();
            bool needSummary = true;
            if (p.contains("summarize")) needSummary = p.value("summarize").toBool();
            else if (obj.contains("summarize")) needSummary = obj.value("summarize").toBool();
            QString description = obj["description"].toString();
            return new CallSequenceCommand(sequenceName, needSummary, description);
        });

        REGISTER_COMMAND(OPEN_SEQUENCE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString sequenceName = firstNonEmptyString(p, {"sequenceName", "sequence", "name", "sequence_name"});
            if (sequenceName.isEmpty()) sequenceName = obj["sequence_name"].toString();
            if (sequenceName.isEmpty()) sequenceName = p["sequence_name"].toString();
            QString description = obj["description"].toString();
            return new OpenSequenceCommand(sequenceName, description);
        });

        /* ---- Directory listing ---- */
        REGISTER_COMMAND(LIST_FILES_IN_FOLDER, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString folderPath = firstNonEmptyString(p, {"folderPath", "path", "folder_path"});
            if (folderPath.isEmpty()) folderPath = obj["folder_path"].toString();
            if (folderPath.isEmpty()) folderPath = p["folder_path"].toString();
            const QString pattern = firstNonEmptyString(p, {"pattern", "filter", "glob"});
            QString description = obj["description"].toString();
            return new ListFilesInFolderCommand(folderPath, pattern, description);
        });

        /* ---- Reasoning ---- */
        REGISTER_COMMAND(REASONING, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            QString reasoning = firstNonEmptyString(p, {"reasoning", "thoughts"});
            if (reasoning.isEmpty()) reasoning = obj["reasoning"].toString();
            if (reasoning.isEmpty()) reasoning = p["thought_process"].toString();
            if (reasoning.isEmpty()) reasoning = p["reasoning_text"].toString();
            QString nextCommand = firstNonEmptyString(p, {"nextCommand", "next_command"});
            if (nextCommand.isEmpty()) nextCommand = obj["next_command"].toString();
            if (nextCommand.isEmpty()) nextCommand = p["next_command"].toString();
            QJsonObject nextCommandParams = objectFromValue(p.value("nextCommandParams"));
            QString description = obj["description"].toString();
            return new ReasoningCommand(reasoning, nextCommand, nextCommandParams, description);
        });

        /* ---- File copy / dir ops ---- */
        REGISTER_COMMAND(COPY_FILE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString from = firstNonEmptyString(p, {"from", "src", "source", "sourcePath", "source_path"});
            const QString to = firstNonEmptyString(p, {"to", "dst", "dest", "destination", "destinationPath", "dest_path"});
            const bool overwrite = p.value("overwrite").toBool(obj.value("overwrite").toBool(false));
            const QString description = obj["description"].toString();
            return new CopyFileCommand(from, to, overwrite, description);
        });

        REGISTER_COMMAND(CREATE_DIR, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString path = firstNonEmptyString(p, {"path", "dir", "folder", "directory"});
            const QString description = obj["description"].toString();
            return new CreateDirCommand(path, description);
        });

        REGISTER_COMMAND(DELETE_DIR, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString path = firstNonEmptyString(p, {"path", "dir", "folder", "directory"});
            const bool recursive = p.value("recursive").toBool(obj.value("recursive").toBool(false));
            const QString description = obj["description"].toString();
            return new DeleteDirCommand(path, recursive, description);
        });

        REGISTER_COMMAND(APPEND_TO_FILE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString path = firstNonEmptyString(p, {"path", "file", "filePath", "file_path"});
            const QString content = firstNonEmptyString(p, {"content", "text", "data"});
            const bool createIfMissing = p.value("createIfMissing").toBool(obj.value("createIfMissing").toBool(true));
            const bool ensureNewline = p.value("ensureNewline").toBool(obj.value("ensureNewline").toBool(false));
            const QString description = obj["description"].toString();
            return new AppendToFileCommand(path, content.toUtf8(), createIfMissing, ensureNewline, description);
        });

        REGISTER_COMMAND(INSERT_AT_LINE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString path = firstNonEmptyString(p, {"path", "file", "filePath", "file_path"});
            const int line = firstNonZeroInt(p, {"line", "lineNumber", "line_number"}, obj.value("line").toInt());
            const QString content = firstNonEmptyString(p, {"content", "text"});
            const QString description = obj["description"].toString();
            return new InsertAtLineCommand(path, line, content.toUtf8(), description);
        });

        REGISTER_COMMAND(APPLY_PATCH, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString patch = firstNonEmptyString(p, {"patch", "diff"});
            const int strip = p.value("strip").toInt(obj.value("strip").toInt(0));
            const QString description = obj["description"].toString();
            return new ApplyPatchCommand(patch.toUtf8(), strip, description);
        });

        /* ---- Git commands ---- */
        REGISTER_COMMAND(GIT_STATUS, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const bool porcelain = p.value("porcelain").toBool(obj.value("porcelain").toBool(true));
            const QString path = firstNonEmptyString(p, {"path", "file"});
            const QString description = obj["description"].toString();
            return new GitStatusCommand(porcelain, path, description);
        });

        REGISTER_COMMAND(GIT_DIFF, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const bool staged = p.value("staged").toBool(obj.value("staged").toBool(false));
            const int contextLines = p.value("contextLines").toInt(obj.value("contextLines").toInt(-1));
            const QString path = firstNonEmptyString(p, {"path", "file"});
            const QString description = obj["description"].toString();
            return new GitDiffCommand(staged, contextLines, path, description);
        });

        REGISTER_COMMAND(GIT_RESTORE_FILE, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString path = firstNonEmptyString(p, {"path", "file"});
            const bool staged = p.value("staged").toBool(obj.value("staged").toBool(false));
            const bool worktree = p.value("worktree").toBool(obj.value("worktree").toBool(true));
            const QString description = obj["description"].toString();
            return new GitRestoreFileCommand(path, staged, worktree, description);
        });

        REGISTER_COMMAND(UNDO_LAST_CHANGE, [](const QJsonObject& obj) {
            const QString description = obj["description"].toString();
            return new UndoLastChangeCommand(description);
        });

        /* ---- Context commands ---- */
        REGISTER_COMMAND(CONTEXT_SHOW, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const int maxChars = p.value("maxChars").toInt(obj.value("maxChars").toInt(8000));
            const QString description = obj["description"].toString();
            return new ContextShowCommand(maxChars, description);
        });

        REGISTER_COMMAND(CONTEXT_CLEAR, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString scope = firstNonEmptyString(p, {"scope"});
            const QString description = obj["description"].toString();
            return new ContextClearCommand(scope, description);
        });

        /* ---- App / URL ---- */
        REGISTER_COMMAND(OPEN_APP, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString app = firstNonEmptyString(p, {"app", "path", "program", "application", "exe"});
            QJsonValue args = p.value("args");
            if (args.isUndefined() || args.isNull()) args = p.value("arguments");
            const QString cwd = firstNonEmptyString(p, {"cwd", "workdir", "workingDirectory"});
            const bool detached = p.value("detached").toBool(obj.value("detached").toBool(true));
            const int waitMs = p.value("waitMs").toInt(obj.value("waitMs").toInt(15000));
            const QString description = obj["description"].toString();
            return new OpenAppCommand(app, args, cwd, detached, waitMs, description);
        });

        REGISTER_COMMAND(OPEN_URL, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString url = firstNonEmptyString(p, {"url", "link", "uri", "address"});
            const QString description = obj["description"].toString();
            return new OpenUrlCommand(url, description);
        });

        /* ---- Clipboard ---- */
        REGISTER_COMMAND(CLIPBOARD_SET, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const QString text = firstNonEmptyString(p, {"text", "content", "value"});
            const QString description = obj["description"].toString();
            return new ClipboardSetCommand(text, description);
        });

        REGISTER_COMMAND(CLIPBOARD_GET, [](const QJsonObject& obj) {
            const QJsonObject p = normalizedParams(obj);
            const int maxChars = p.value("maxChars").toInt(obj.value("maxChars").toInt(4000));
            const QString description = obj["description"].toString();
            return new ClipboardGetCommand(maxChars, description);
        });

#undef REGISTER_COMMAND
    }
    return s_registry;
}

/* ------------------------------------------------------------------ */
/*  all() - unchanged                                                  */
/* ------------------------------------------------------------------ */

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
        new ContextClearCommand(QStringLiteral("active")),
        new OpenAppCommand(QString()),
        new OpenUrlCommand(QString()),
        new ClipboardSetCommand(QString()),
        new ClipboardGetCommand(4000)
    };
}

/* ------------------------------------------------------------------ */
/*  create() - now just a hash lookup                                  */
/* ------------------------------------------------------------------ */

Command* CommandRegistry::create(const QJsonObject& obj)
{
    QString cmd = firstNonEmptyString(obj, {"command", "action", "name", "tool", "type"});
    cmd = canonicalCommandName(cmd);

    auto it = registry().find(cmd);
    if (it != registry().end())
        return it.value()(obj);

    return nullptr;
}
