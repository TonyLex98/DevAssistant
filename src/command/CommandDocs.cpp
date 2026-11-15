#include "CommandDocs.h"

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
        d.summary = QStringLiteral("Задаёт пользователю уточняющий вопрос (используй, если не хватает данных).");
        d.params = { req("question", "string", QStringLiteral("Текст вопроса (что именно нужно уточнить).")) };
        d.notes = QStringList() << QStringLiteral("Не добавляй лишних команд: сначала спроси, потом действуй по ответу.");
        d.example = baseExample(QJsonObject{{"question", "Какой файл менять: src/main.cpp или src/app.cpp?"}});
        return d;
    }

    if (commandName == "OPEN_FILE") {
        d.summary = QStringLiteral("Открывает файл и возвращает содержимое с номерами строк.");
        d.params = { req("path", "string", QStringLiteral("Путь к файлу (абсолютный или относительно корня проекта).")) };
        d.notes = QStringList()
                  << QStringLiteral("Номера строк из результата используй для EDIT_IN_RANGE.")
                  << QStringLiteral("Если файл большой — используй OPEN_FILE_REGION.");
        d.example = baseExample(QJsonObject{{"path", "src/main.cpp"}});
        return d;
    }

    if (commandName == "OPEN_FILE_REGION") {
        d.summary = QStringLiteral("Открывает часть файла по диапазону строк или по regex и возвращает строки с номерами.");
        d.params = {
            req("path", "string", QStringLiteral("Путь к файлу (абсолютный или относительно корня проекта).")),
            opt("startLine", "int", QStringLiteral("Начальная строка (1-based, включительно). Требуется вместе с endLine.")),
            opt("endLine", "int", QStringLiteral("Конечная строка (1-based, включительно). Требуется вместе с startLine.")),
            opt("region", "string", QStringLiteral("Regex: если указан, вернёт строки, которые совпали (вместо диапазона)."))
        };
        d.notes = QStringList() << QStringLiteral("Нужно указать либо startLine+endLine, либо region.");
        d.example = baseExample(QJsonObject{{"path", "src/main.cpp"}, {"startLine", 1}, {"endLine", 120}});
        return d;
    }

    if (commandName == "CREATE_FILE") {
        d.summary = QStringLiteral("Создаёт новый текстовый файл. Ошибка, если файл уже существует.");
        d.params = {
            req("path", "string", QStringLiteral("Путь к новому файлу (абсолютный или относительно корня проекта).")),
            req("content", "string", QStringLiteral("Полное содержимое файла (используй \\n для переносов)."))
        };
        d.notes = QStringList()
                  << QStringLiteral("Не используй для перезаписи существующего файла — для этого REPLACE_IN_FILE или EDIT_IN_RANGE.")
                  << QStringLiteral("Если нужно создать папки — они будут созданы автоматически.");
        d.example = baseExample(QJsonObject{
            {"path", "src/utils/NewHelper.h"},
            {"content", "#pragma once\n\nint add(int a, int b);\n"}
        });
        return d;
    }

    if (commandName == "REPLACE_IN_FILE") {
        d.summary = QStringLiteral("Полностью заменяет содержимое существующего файла на новое.");
        d.params = {
            req("path", "string", QStringLiteral("Путь к файлу (абсолютный или относительно корня проекта).")),
            req("content", "string", QStringLiteral("Новое полное содержимое файла (используй \\n)."))
        };
        d.notes = QStringList() << QStringLiteral("Если нужно точечно поменять строки — используй EDIT_IN_RANGE.");
        d.example = baseExample(QJsonObject{
            {"path", "src/main.cpp"},
            {"content", "#include <iostream>\n\nint main(){\n  std::cout << \"Hello\";\n}\n"}
        });
        return d;
    }

    if (commandName == "EDIT_IN_RANGE") {
        d.summary = QStringLiteral("Заменяет диапазон строк в файле (по номерам строк из OPEN_FILE/OPEN_FILE_REGION).");
        d.params = {
            req("path", "string", QStringLiteral("Путь к файлу (абсолютный или относительно корня проекта).")),
            req("startLine", "int", QStringLiteral("Начальная строка (1-based, включительно).")),
            req("endLine", "int", QStringLiteral("Конечная строка (1-based, включительно).")),
            opt("content", "string", QStringLiteral("Новые строки одним текстом (\\n). Альтернатива replacementLines.")),
            opt("replacementLines", "array", QStringLiteral("Массив объектов {line:int, text:string}. Альтернатива content."))
        };
        d.notes = QStringList()
                  << QStringLiteral("Проще всего: укажи startLine/endLine и content с нужными строками.")
                  << QStringLiteral("Диапазон обязан существовать в файле; иначе будет ошибка.");
        d.example = baseExample(QJsonObject{
            {"path", "src/main.cpp"},
            {"startLine", 10},
            {"endLine", 12},
            {"content", "int x = 1;\nint y = 2;\n"}
        });
        return d;
    }

    if (commandName == "DELETE_FILE") {
        d.summary = QStringLiteral("Удаляет файл.");
        d.params = { req("path", "string", QStringLiteral("Путь к файлу (абсолютный или относительно корня проекта).")) };
        d.notes = QStringList() << QStringLiteral("Используй осторожно. Если не уверен — сначала OPEN_FILE.");
        d.example = baseExample(QJsonObject{{"path", "src/old/unused.cpp"}});
        return d;
    }

    if (commandName == "RENAME_FILE") {
        d.summary = QStringLiteral("Переименовывает/перемещает файл.");
        d.params = {
            req("oldPath", "string", QStringLiteral("Старый путь к файлу.")),
            req("newPath", "string", QStringLiteral("Новый путь к файлу."))
        };
        d.notes = QStringList() << QStringLiteral("Пути могут быть абсолютными или относительно корня проекта.");
        d.example = baseExample(QJsonObject{{"oldPath", "src/old_name.cpp"}, {"newPath", "src/new_name.cpp"}});
        return d;
    }

    if (commandName == "WRITE_DESCRIPTION") {
        d.summary = QStringLiteral("Записывает текстовое описание для файла (метаданные проекта).");
        d.params = {
            req("sourceFilePath", "string", QStringLiteral("Файл, к которому относится описание (абсолютный или относительный путь).")),
            req("description", "string", QStringLiteral("Описание файла (что делает и где используется)."))
        };
        d.notes = QStringList() << QStringLiteral("Полезно после больших изменений: чтобы ассистент лучше ориентировался в проекте.");
        d.example = baseExample(QJsonObject{
            {"sourceFilePath", "src/main.cpp"},
            {"description", "Точка входа приложения, инициализация UI и менеджеров."}
        });
        return d;
    }

    if (commandName == "RUN_PYTHON") {
        d.summary = QStringLiteral("Запускает Python-скрипт (scriptPath) или inline-код (code) и возвращает stdout/stderr/exitCode.");
        d.params = {
            opt("scriptPath", "string", QStringLiteral("Путь к .py файлу (абсолютный или относительно корня проекта).")),
            opt("code", "string", QStringLiteral("Inline Python-код (будет выполнен через `python -c`).")),
            opt("cwd", "string", QStringLiteral("Рабочая директория (абсолютная или относительно корня проекта).")),
            opt("timeoutMs", "int", QStringLiteral("Лимит выполнения в миллисекундах (по умолчанию 120000)."))
        };
        d.notes = QStringList()
                  << QStringLiteral("Нужно указать либо scriptPath, либо code.")
                  << QStringLiteral("Печатай результат через print().");
        d.example = baseExample(QJsonObject{
            {"code", "print(1+1)\n"},
            {"cwd", "."}
        });
        return d;
    }

    if (commandName == "RUN_SHELL") {
        d.summary = QStringLiteral("Запускает команду в shell и возвращает stdout/stderr/exitCode (рабочая папка: корень проекта).");
        d.params = {
            req("command", "string", QStringLiteral("Команда для выполнения (одной строкой).")),
            opt("cwd", "string", QStringLiteral("Рабочая директория (абсолютная или относительно корня проекта).")),
            opt("timeoutMs", "int", QStringLiteral("Лимит выполнения в миллисекундах (по умолчанию 120000)."))
        };
        d.notes = QStringList()
                  << QStringLiteral("Для поиска по проекту предпочитай SEARCH_PROJECT.")
                  << QStringLiteral("Если нужен запуск в другой папке — укажи cwd.");
        d.example = baseExample(QJsonObject{{"command", "dir"}, {"cwd", "."}});
        return d;
    }

    if (commandName == "SEARCH_WEB") {
        d.summary = QStringLiteral("Ищет информацию в интернете и возвращает краткие результаты (title/url/snippet).");
        d.params = {
            req("query", "string", QStringLiteral("Поисковый запрос.")),
            opt("maxResults", "int", QStringLiteral("Сколько результатов вернуть (1..10)."))
        };
        d.example = baseExample(QJsonObject{{"query", "Qt 5.12 QNetworkAccessManager timeout example"}, {"maxResults", 5}});
        return d;
    }

    if (commandName == "SEARCH_PROJECT") {
        d.summary = QStringLiteral("Ищет текст/regex по файлам проекта и возвращает совпадения вида path:line + snippet.");
        d.params = {
            req("query", "string", QStringLiteral("Строка поиска или regex (если useRegex=true).")),
            opt("maxMatches", "int", QStringLiteral("Максимум совпадений (по умолчанию 10).")),
            opt("caseSensitive", "bool", QStringLiteral("Учитывать регистр (по умолчанию false).")),
            opt("useRegex", "bool", QStringLiteral("Интерпретировать query как regex.")),
            opt("contextLines", "int", QStringLiteral("Сколько строк контекста вокруг совпадения добавить.")),
            opt("include", "array<string>", QStringLiteral("Глоб-маски файлов, например [\"*.cpp\",\"*.h\"].")),
            opt("includePaths", "array<string>", QStringLiteral("Ограничить поиск указанными путями/папками.")),
            opt("excludePaths", "array<string>", QStringLiteral("Исключить пути/папки."))
        };
        d.example = baseExample(QJsonObject{
            {"query", "observeUserQuery"},
            {"maxMatches", 20},
            {"include", QJsonArray{ "*.cpp", "*.h" }}
        });
        return d;
    }

    if (commandName == "SEARCH_PROJECT_CASE_INSENSITIVE") {
        d.summary = QStringLiteral("Упрощённый поиск по проекту без учёта регистра.");
        d.params = {
            req("query", "string", QStringLiteral("Строка поиска.")),
            opt("maxMatches", "int", QStringLiteral("Максимум совпадений (по умолчанию 10).")),
            opt("include", "array<string>", QStringLiteral("Глоб-маски файлов, например [\"*.cpp\",\"*.h\"]."))
        };
        d.example = baseExample(QJsonObject{
            {"query", "SearchWebCommand"},
            {"maxMatches", 10},
            {"include", QJsonArray{ "*.cpp", "*.h" }}
        });
        return d;
    }

    if (commandName == "LIST_FILES_IN_FOLDER") {
        d.summary = QStringLiteral("Список файлов в директории (не рекурсивно) + количество строк в каждом файле.");
        d.params = {
            req("folderPath", "string", QStringLiteral("Путь к папке (абсолютный или относительно корня проекта).")),
            opt("pattern", "string", QStringLiteral("Фильтр по имени, например \"*.cpp\" (необязательно)."))
        };
        d.notes = QStringList()
                  << QStringLiteral("Команда перечисляет файлы в папке (не рекурсивно) и считает строки в текстовых файлах.");
        d.example = baseExample(QJsonObject{{"folderPath", "DevAss/src/command"}, {"pattern", "*.cpp"}});
        return d;
    }

    if (commandName == "SHOW_RESULT") {
        d.summary = QStringLiteral("Показывает пользователю итоговый текст/ответ (без выполнения действий).");
        d.params = {
            req("content", "string", QStringLiteral("Текст, который нужно показать пользователю.")),
            opt("title", "string", QStringLiteral("Заголовок (необязательно)."))
        };
        d.example = baseExample(QJsonObject{{"title", "Итог"}, {"content", "Сделано: ...\nДальше: ..."}});
        return d;
    }

    if (commandName == "CALL_SEQUENCE") {
        d.summary = QStringLiteral("Передаёт управление другой последовательности/эксперту (оркестрация).");
        d.params = {
            req("sequenceName", "string", QStringLiteral("Имя последовательности, как в списке {{OPEN_SEQUENCE_INFO}}.")),
            opt("summarize", "bool", QStringLiteral("Сделать резюме перед переходом (если поддерживается)."))
        };
        d.example = baseExample(QJsonObject{{"sequenceName", "InternetFinder"}, {"summarize", true}});
        return d;
    }

    if (commandName == "OPEN_SEQUENCE") {
        d.summary = QStringLiteral("Открывает (загружает) последовательность по имени.");
        d.params = { req("sequenceName", "string", QStringLiteral("Имя последовательности (как в списке).")) };
        d.example = baseExample(QJsonObject{{"sequenceName", "InternetFinder"}});
        return d;
    }

    if (commandName == "REASONING") {
        d.summary = QStringLiteral("Логирует рассуждение/план (без действий).");
        d.params = {
            req("reasoning", "string", QStringLiteral("Короткий план: что делать дальше и почему.")),
            opt("nextCommand", "string", QStringLiteral("Подсказка: какая команда будет следующей.")),
            opt("nextCommandParams", "object", QStringLiteral("Подсказка: params для nextCommand."))
        };
        d.example = baseExample(QJsonObject{
            {"reasoning", "Сначала открываю файл, затем меняю диапазон строк и проверяю сборку."},
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
        d.notes = { QStringLiteral("Works after the project was indexed in Settings > Documents; otherwise it fails with that hint.") };
        d.example = baseExample(QJsonObject{{"query", "how are settings saved"}, {"k", 5}});
        return d;
    }

    if (commandName == "CLIPBOARD_GET") {
        d.summary = QStringLiteral("Reads text from the system clipboard.");
        d.params = { opt("maxChars", "int", QStringLiteral("Limit returned text size (default 4000).")) };
        d.example = baseExample(QJsonObject{{"maxChars", 2000}});
        return d;
    }

    d.summary = QStringLiteral("Команда проекта.");
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
