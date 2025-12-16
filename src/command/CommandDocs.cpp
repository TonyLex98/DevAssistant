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
            opt("cwd", "string", QStringLiteral("Рабочая директория (абсолютная или относительно корня проекта)."))
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
            opt("cwd", "string", QStringLiteral("Рабочая директория (абсолютная или относительно корня проекта)."))
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

    d.summary = QStringLiteral("Команда проекта.");
    d.example = baseExample(QJsonObject());
    return d;
}

QString CommandDocs::renderForLLM(const CommandDoc& doc)
{
    QString out;
    out += "- " + doc.name + ": " + doc.summary + "\n";

    if (!doc.params.isEmpty()) {
        out += "  Params:\n";
        for (const auto& p : doc.params) {
            out += QString("    - %1 (%2%3): %4\n")
                       .arg(p.name, p.type, p.required ? ", required" : ", optional", p.description);
        }
    }

    out += "  JSON: " + compactJson(doc.example) + "\n";

    if (!doc.notes.isEmpty()) {
        out += "  Notes:\n";
        for (const auto& note : doc.notes) {
            out += "    - " + note + "\n";
        }
    }

    return out.trimmed();
}
