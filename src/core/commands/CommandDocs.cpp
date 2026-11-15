#include "core/commands/CommandDocs.h"

#include <QJsonArray>
#include <QJsonDocument>

static CommandParamDoc req(const QString& name, const QString& type, const QString& description)
{
    CommandParamDoc out;
    out.name = name;
    out.type = type;
    out.required = true;
    out.description = description;
    return out;
}

static CommandParamDoc opt(const QString& name, const QString& type, const QString& description)
{
    CommandParamDoc out;
    out.name = name;
    out.type = type;
    out.required = false;
    out.description = description;
    return out;
}

QString CommandDocs::compactJson(const QJsonObject& obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

CommandDoc CommandDocs::doc(const QString& commandName)
{
    CommandDoc d;
    d.name = commandName;

    auto baseExample = [&](const QJsonObject& params) {
        QJsonObject obj;
        obj["command"] = d.name;
        obj["params"] = params;
        return obj;
    };

    if (commandName == "ASK_USER") {
        d.summary = QStringLiteral("Asks the user a clarifying question (use it when information is missing).");
        d.params = { req("question", "string", QStringLiteral("Question text (what exactly needs clarifying).")) };
        d.notes = QStringList() << QStringLiteral("Do not add other commands: ask first, then act on the answer.");
        d.example = baseExample(QJsonObject{{"question", "Which file should be changed: src/main.cpp or src/app.cpp?"}});
        return d;
    }

    if (commandName == "OPEN_FILE") {
        d.summary = QStringLiteral("Opens a file and returns its contents with line numbers.");
        d.params = { req("path", "string", QStringLiteral("File path (absolute or relative to the project root).")) };
        d.notes = QStringList()
                  << QStringLiteral("Use the line numbers from the result for EDIT_IN_RANGE.")
                  << QStringLiteral("For a large file, use OPEN_FILE_REGION.");
        d.example = baseExample(QJsonObject{{"path", "src/main.cpp"}});
        return d;
    }

    if (commandName == "OPEN_FILE_REGION") {
        d.summary = QStringLiteral("Opens part of a file by line range or by regex and returns the lines with numbers.");
        d.params = {
            req("path", "string", QStringLiteral("File path (absolute or relative to the project root).")),
            opt("startLine", "int", QStringLiteral("First line (1-based, inclusive). Requires endLine.")),
            opt("endLine", "int", QStringLiteral("Last line (1-based, inclusive). Requires startLine.")),
            opt("region", "string", QStringLiteral("Regex: if set, returns the matching lines (instead of a range)."))
        };
        d.notes = QStringList() << QStringLiteral("Specify either startLine+endLine or region.");
        d.example = baseExample(QJsonObject{{"path", "src/main.cpp"}, {"startLine", 1}, {"endLine", 120}});
        return d;
    }

    if (commandName == "CREATE_FILE") {
        d.summary = QStringLiteral("Creates a new text file. Fails if the file already exists.");
        d.params = {
            req("path", "string", QStringLiteral("Path of the new file (absolute or relative to the project root).")),
            req("content", "string", QStringLiteral("Full file contents (use \\n for line breaks)."))
        };
        d.notes = QStringList()
                  << QStringLiteral("Do not use it to overwrite an existing file; use REPLACE_IN_FILE or EDIT_IN_RANGE for that.")
                  << QStringLiteral("Missing folders are created automatically.");
        d.example = baseExample(QJsonObject{
            {"path", "src/utils/NewHelper.h"},
            {"content", "#pragma once\n\nint add(int a, int b);\n"}
        });
        return d;
    }

    if (commandName == "REPLACE_IN_FILE") {
        d.summary = QStringLiteral("Replaces the entire contents of an existing file.");
        d.params = {
            req("path", "string", QStringLiteral("File path (absolute or relative to the project root).")),
            req("content", "string", QStringLiteral("New full file contents (use \\n)."))
        };
        d.notes = QStringList() << QStringLiteral("To change only specific lines, use EDIT_IN_RANGE.");
        d.example = baseExample(QJsonObject{
            {"path", "src/main.cpp"},
            {"content", "#include <iostream>\n\nint main(){\n  std::cout << \"Hello\";\n}\n"}
        });
        return d;
    }

    if (commandName == "EDIT_IN_RANGE") {
        d.summary = QStringLiteral("Replaces a range of lines in a file (line numbers from OPEN_FILE/OPEN_FILE_REGION).");
        d.params = {
            req("path", "string", QStringLiteral("File path (absolute or relative to the project root).")),
            req("startLine", "int", QStringLiteral("First line (1-based, inclusive).")),
            req("endLine", "int", QStringLiteral("Last line (1-based, inclusive).")),
            opt("content", "string", QStringLiteral("New lines as a single text (\\n). Alternative to replacementLines.")),
            opt("replacementLines", "array", QStringLiteral("Array of objects {line:int, text:string}. Alternative to content."))
        };
        d.notes = QStringList()
                  << QStringLiteral("Simplest: set startLine/endLine and content with the new lines.")
                  << QStringLiteral("The range must exist in the file; otherwise it is an error.");
        d.example = baseExample(QJsonObject{
            {"path", "src/main.cpp"},
            {"startLine", 10},
            {"endLine", 12},
            {"content", "int x = 1;\nint y = 2;\n"}
        });
        return d;
    }

    if (commandName == "DELETE_FILE") {
        d.summary = QStringLiteral("Deletes a file.");
        d.params = { req("path", "string", QStringLiteral("File path (absolute or relative to the project root).")) };
        d.notes = QStringList() << QStringLiteral("Use with care. If unsure, run OPEN_FILE first.");
        d.example = baseExample(QJsonObject{{"path", "src/old/unused.cpp"}});
        return d;
    }

    if (commandName == "RENAME_FILE") {
        d.summary = QStringLiteral("Renames or moves a file.");
        d.params = {
            req("oldPath", "string", QStringLiteral("Current file path.")),
            req("newPath", "string", QStringLiteral("New file path."))
        };
        d.notes = QStringList() << QStringLiteral("Paths may be absolute or relative to the project root.");
        d.example = baseExample(QJsonObject{{"oldPath", "src/old_name.cpp"}, {"newPath", "src/new_name.cpp"}});
        return d;
    }

    if (commandName == "WRITE_DESCRIPTION") {
        d.summary = QStringLiteral("Writes a text description for a file (project metadata).");
        d.params = {
            req("sourceFilePath", "string", QStringLiteral("File the description belongs to (absolute or relative path).")),
            req("description", "string", QStringLiteral("File description (what it does and where it is used)."))
        };
        d.notes = QStringList() << QStringLiteral("Useful after large changes, so the assistant finds its way around the project better.");
        d.example = baseExample(QJsonObject{
            {"sourceFilePath", "src/main.cpp"},
            {"description", "Application entry point; initializes the UI and managers."}
        });
        return d;
    }

    if (commandName == "RUN_PYTHON") {
        d.summary = QStringLiteral("Runs a Python script (scriptPath) or inline code (code) and returns stdout/stderr/exitCode.");
        d.params = {
            opt("scriptPath", "string", QStringLiteral("Path to a .py file (absolute or relative to the project root).")),
            opt("code", "string", QStringLiteral("Inline Python code (run via `python -c`).")),
            opt("cwd", "string", QStringLiteral("Working directory (absolute or relative to the project root).")),
            opt("timeoutMs", "int", QStringLiteral("Time limit in milliseconds (default 120000)."))
        };
        d.notes = QStringList()
                  << QStringLiteral("Specify either scriptPath or code.")
                  << QStringLiteral("Print the result with print().");
        d.example = baseExample(QJsonObject{
            {"code", "print(1+1)\n"},
            {"cwd", "."}
        });
        return d;
    }

    if (commandName == "RUN_SHELL") {
        d.summary = QStringLiteral("Runs a shell command and returns stdout/stderr/exitCode (working folder: the project root).");
        d.params = {
            req("command", "string", QStringLiteral("Command to run (a single line).")),
            opt("cwd", "string", QStringLiteral("Working directory (absolute or relative to the project root).")),
            opt("timeoutMs", "int", QStringLiteral("Time limit in milliseconds (default 120000)."))
        };
        d.notes = QStringList()
                  << QStringLiteral("To search the project, prefer SEARCH_PROJECT.")
                  << QStringLiteral("To run in another folder, set cwd.");
        d.example = baseExample(QJsonObject{{"command", "dir"}, {"cwd", "."}});
        return d;
    }

    if (commandName == "SEARCH_WEB") {
        d.summary = QStringLiteral("Searches the web and returns short results (title/url/snippet).");
        d.params = {
            req("query", "string", QStringLiteral("Search query.")),
            opt("maxResults", "int", QStringLiteral("How many results to return (1..10)."))
        };
        d.example = baseExample(QJsonObject{{"query", "Qt 5.12 QNetworkAccessManager timeout example"}, {"maxResults", 5}});
        return d;
    }

    if (commandName == "SEARCH_PROJECT") {
        d.summary = QStringLiteral("Searches project files for text/regex and returns matches as path:line + snippet.");
        d.params = {
            req("query", "string", QStringLiteral("Search string or regex (if useRegex=true).")),
            opt("maxMatches", "int", QStringLiteral("Maximum number of matches (default 10).")),
            opt("caseSensitive", "bool", QStringLiteral("Match case (default false).")),
            opt("useRegex", "bool", QStringLiteral("Treat query as a regex.")),
            opt("contextLines", "int", QStringLiteral("How many context lines to add around each match.")),
            opt("include", "array<string>", QStringLiteral("File glob masks, e.g. [\"*.cpp\",\"*.h\"].")),
            opt("includePaths", "array<string>", QStringLiteral("Limit the search to these paths/folders.")),
            opt("excludePaths", "array<string>", QStringLiteral("Paths/folders to exclude."))
        };
        d.example = baseExample(QJsonObject{
            {"query", "observeUserQuery"},
            {"maxMatches", 20},
            {"include", QJsonArray{ "*.cpp", "*.h" }}
        });
        return d;
    }

    if (commandName == "SEARCH_PROJECT_CASE_INSENSITIVE") {
        d.summary = QStringLiteral("Simplified case-insensitive project search.");
        d.params = {
            req("query", "string", QStringLiteral("Search string.")),
            opt("maxMatches", "int", QStringLiteral("Maximum number of matches (default 10).")),
            opt("include", "array<string>", QStringLiteral("File glob masks, e.g. [\"*.cpp\",\"*.h\"]."))
        };
        d.example = baseExample(QJsonObject{
            {"query", "SearchWebCommand"},
            {"maxMatches", 10},
            {"include", QJsonArray{ "*.cpp", "*.h" }}
        });
        return d;
    }

    if (commandName == "LIST_FILES_IN_FOLDER") {
        d.summary = QStringLiteral("Lists the files in a folder (non-recursive) with the line count of each file.");
        d.params = {
            req("folderPath", "string", QStringLiteral("Folder path (absolute or relative to the project root).")),
            opt("pattern", "string", QStringLiteral("Name filter, e.g. \"*.cpp\" (optional)."))
        };
        d.notes = QStringList()
                  << QStringLiteral("The command lists the files in the folder (non-recursive) and counts lines in text files.");
        d.example = baseExample(QJsonObject{{"folderPath", "DevAss/src/command"}, {"pattern", "*.cpp"}});
        return d;
    }

    if (commandName == "SHOW_RESULT") {
        d.summary = QStringLiteral("Shows the final text/answer to the user (performs no actions).");
        d.params = {
            req("content", "string", QStringLiteral("Text to show to the user.")),
            opt("title", "string", QStringLiteral("Title (optional)."))
        };
        d.example = baseExample(QJsonObject{{"title", "Summary"}, {"content", "Done: ...\nNext: ..."}});
        return d;
    }

    if (commandName == "CALL_SEQUENCE") {
        d.summary = QStringLiteral("Hands control over to another sequence/expert (orchestration).");
        d.params = {
            req("sequenceName", "string", QStringLiteral("Sequence name as listed in {{OPEN_SEQUENCE_INFO}}.")),
            opt("summarize", "bool", QStringLiteral("Summarize before switching (if supported)."))
        };
        d.example = baseExample(QJsonObject{{"sequenceName", "InternetFinder"}, {"summarize", true}});
        return d;
    }

    if (commandName == "OPEN_SEQUENCE") {
        d.summary = QStringLiteral("Opens (loads) a sequence by name.");
        d.params = { req("sequenceName", "string", QStringLiteral("Sequence name (as listed).")) };
        d.example = baseExample(QJsonObject{{"sequenceName", "InternetFinder"}});
        return d;
    }

    if (commandName == "REASONING") {
        d.summary = QStringLiteral("Logs reasoning/a plan (performs no actions).");
        d.params = {
            req("reasoning", "string", QStringLiteral("Short plan: what to do next and why.")),
            opt("nextCommand", "string", QStringLiteral("Hint: which command comes next.")),
            opt("nextCommandParams", "object", QStringLiteral("Hint: params for nextCommand."))
        };
        d.example = baseExample(QJsonObject{
            {"reasoning", "First open the file, then replace the line range and check the build."},
            {"nextCommand", "OPEN_FILE"},
            {"nextCommandParams", QJsonObject{{"path", "src/main.cpp"}}}
        });
        return d;
    }

    if (commandName == "COPY_FILE") {
        d.summary = QStringLiteral("Copies a file (optionally overwriting the destination).");
        d.params = {
            req("from", "string", QStringLiteral("Source file path (relative to project root or absolute).")),
            req("to", "string", QStringLiteral("Destination file path (relative to project root or absolute).")),
            opt("overwrite", "bool", QStringLiteral("If true, overwrites destination if it exists."))
        };
        d.example = baseExample(QJsonObject{{"from", "src/a.txt"}, {"to", "src/b.txt"}, {"overwrite", false}});
        return d;
    }

    if (commandName == "CREATE_DIR") {
        d.summary = QStringLiteral("Creates a directory (recursively).");
        d.params = { req("path", "string", QStringLiteral("Directory path (relative to project root or absolute).")) };
        d.example = baseExample(QJsonObject{{"path", "src/new_folder"}});
        return d;
    }

    if (commandName == "DELETE_DIR") {
        d.summary = QStringLiteral("Deletes a directory (empty by default).");
        d.params = {
            req("path", "string", QStringLiteral("Directory path (relative to project root or absolute).")),
            opt("recursive", "bool", QStringLiteral("If true, deletes directory contents (UNDO may fail)."))
        };
        d.example = baseExample(QJsonObject{{"path", "build/tmp"}, {"recursive", false}});
        return d;
    }

    if (commandName == "APPEND_TO_FILE") {
        d.summary = QStringLiteral("Appends text to a file (optionally creating it).");
        d.params = {
            req("path", "string", QStringLiteral("File path.")),
            req("content", "string", QStringLiteral("Text to append.")),
            opt("createIfMissing", "bool", QStringLiteral("If true, creates file if missing (default true).")),
            opt("ensureNewline", "bool", QStringLiteral("If true, inserts a newline between old content and appended content when needed."))
        };
        d.example = baseExample(QJsonObject{{"path", "README.md"}, {"content", "\nMore..."}, {"createIfMissing", true}});
        return d;
    }

    if (commandName == "INSERT_AT_LINE") {
        d.summary = QStringLiteral("Inserts text before the given 1-based line number.");
        d.params = {
            req("path", "string", QStringLiteral("File path.")),
            req("line", "int", QStringLiteral("1-based line number to insert before. Use OPEN_FILE first.")),
            req("content", "string", QStringLiteral("Text to insert (may be multi-line)."))
        };
        d.example = baseExample(QJsonObject{{"path", "src/main.cpp"}, {"line", 10}, {"content", "// inserted\n"}});
        return d;
    }

    if (commandName == "APPLY_PATCH") {
        d.summary = QStringLiteral("Applies a unified diff (like git diff) to files.");
        d.params = {
            req("patch", "string", QStringLiteral("Unified diff text.")),
            opt("strip", "int", QStringLiteral("Strip N leading path components (like patch -p). Default 0."))
        };
        d.example = baseExample(QJsonObject{{"patch", "diff --git a/a.txt b/a.txt\n--- a/a.txt\n+++ b/a.txt\n@@ -1 +1 @@\n-old\n+new\n"}, {"strip", 0}});
        return d;
    }

    if (commandName == "GIT_STATUS") {
        d.summary = QStringLiteral("Runs 'git status' in the project root.");
        d.params = {
            opt("porcelain", "bool", QStringLiteral("If true, uses --porcelain=v1 -b (default true).")),
            opt("path", "string", QStringLiteral("Optional path filter."))
        };
        d.example = baseExample(QJsonObject{{"porcelain", true}});
        return d;
    }

    if (commandName == "GIT_DIFF") {
        d.summary = QStringLiteral("Runs 'git diff' in the project root.");
        d.params = {
            opt("staged", "bool", QStringLiteral("If true, uses --staged.")),
            opt("contextLines", "int", QStringLiteral("If set, uses -U<contextLines>.")),
            opt("path", "string", QStringLiteral("Optional path filter."))
        };
        d.example = baseExample(QJsonObject{{"staged", false}});
        return d;
    }

    if (commandName == "GIT_RESTORE_FILE") {
        d.summary = QStringLiteral("Restores a file from git (like 'git restore').");
        d.params = {
            req("path", "string", QStringLiteral("File path.")),
            opt("staged", "bool", QStringLiteral("If true, restores staged state.")),
            opt("worktree", "bool", QStringLiteral("If false, attempts staged-only restore."))
        };
        d.notes = QStringList() << QStringLiteral("This change is performed by git; UNDO_LAST_CHANGE may not revert it.");
        d.example = baseExample(QJsonObject{{"path", "src/main.cpp"}, {"staged", false}});
        return d;
    }

    if (commandName == "UNDO_LAST_CHANGE") {
        d.summary = QStringLiteral("Undoes the last applied filesystem change batch (single level).");
        d.params = {};
        d.example = baseExample(QJsonObject());
        return d;
    }

    if (commandName == "CONTEXT_SHOW") {
        d.summary = QStringLiteral("Shows current context buffer (UI-level).");
        d.params = { opt("maxChars", "int", QStringLiteral("Max characters to show (default 8000).")) };
        d.example = baseExample(QJsonObject{{"maxChars", 8000}});
        return d;
    }

    if (commandName == "CONTEXT_CLEAR") {
        d.summary = QStringLiteral("Clears context caches (UI-level).");
        d.params = { opt("scope", "string", QStringLiteral("active|all (default active).")) };
        d.example = baseExample(QJsonObject{{"scope", "active"}});
        return d;
    }

    if (commandName == "OPEN_APP") {
        d.summary = QStringLiteral("Launches a desktop application.");
        d.params = {
            req("app", "string", QStringLiteral("Executable name or absolute path.")),
            opt("args", "array|string", QStringLiteral("Command line arguments.")),
            opt("cwd", "string", QStringLiteral("Working directory.")),
            opt("detached", "bool", QStringLiteral("Launch detached (default true).")),
            opt("waitMs", "int", QStringLiteral("When detached=false, max wait time for process finish (default 15000)."))
        };
        d.example = baseExample(QJsonObject{{"app", "notepad.exe"}, {"args", QJsonArray{"README.md"}}, {"detached", true}});
        return d;
    }

    if (commandName == "OPEN_URL") {
        d.summary = QStringLiteral("Opens a URL in the default system browser.");
        d.params = { req("url", "string", QStringLiteral("URL to open.")) };
        d.example = baseExample(QJsonObject{{"url", "https://example.com"}});
        return d;
    }

    if (commandName == "CLIPBOARD_SET") {
        d.summary = QStringLiteral("Writes text to the system clipboard.");
        d.params = { req("text", "string", QStringLiteral("Text to copy.")) };
        d.example = baseExample(QJsonObject{{"text", "Build command copied"}});
        return d;
    }

    if (commandName == "SEARCH_DOCS") {
        d.summary = QStringLiteral("Semantic search in the project's indexed documents (code, notes, specs, PDFs). "
                                   "Returns the closest excerpts with file names; cite them in the answer.");
        d.params = {
            req("query", "string", QStringLiteral("What to look for, in natural language.")),
            opt("k", "int", QStringLiteral("How many excerpts to return (default 5, at most 20)."))
        };
        d.notes = QStringList() << QStringLiteral("Works after the project was indexed in Settings > Documents; otherwise it fails with that hint.");
        d.example = baseExample(QJsonObject{{"query", "how are settings saved"}, {"k", 5}});
        return d;
    }

    if (commandName == "CLIPBOARD_GET") {
        d.summary = QStringLiteral("Reads text from the system clipboard.");
        d.params = { opt("maxChars", "int", QStringLiteral("Limit returned text size (default 4000).")) };
        d.example = baseExample(QJsonObject{{"maxChars", 2000}});
        return d;
    }

    d.summary = QStringLiteral("Project command.");
    d.example = baseExample(QJsonObject());
    return d;
}

QString CommandDocs::renderForLLM(const CommandDoc& doc)
{
    return renderForLLM(doc, false);
}

QString CommandDocs::renderForLLM(const CommandDoc& doc, bool compact)
{
    QString out;
    out += "- " + doc.name + ": " + doc.summary.trimmed() + "\n";

    if (!doc.params.isEmpty()) {
        out += "  Params:\n";
        const int maxParams = compact ? 6 : 1000;
        for (int i = 0; i < doc.params.size() && i < maxParams; ++i) {
            const auto& p = doc.params[i];
            if (compact) {
                out += QString("    - %1 (%2%3)\n")
                           .arg(p.name, p.type, p.required ? ", required" : ", optional");
            } else {
                out += QString("    - %1 (%2%3): %4\n")
                           .arg(p.name, p.type, p.required ? ", required" : ", optional", p.description);
            }
        }
        if (compact && doc.params.size() > maxParams) {
            out += QString("    - ... (%1 more)\n").arg(doc.params.size() - maxParams);
        }
    }

    out += "  JSON: " + compactJson(doc.example) + "\n";

    if (!doc.notes.isEmpty() && !compact) {
        out += "  Notes:\n";
        for (const auto& note : doc.notes) {
            out += "    - " + note + "\n";
        }
    } else if (compact && !doc.notes.isEmpty()) {
        // Keep a single critical note if present.
        out += "  Notes: " + doc.notes.first().trimmed() + "\n";
    }

    return out.trimmed();
}
