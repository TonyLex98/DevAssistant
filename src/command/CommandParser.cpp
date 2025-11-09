// CommandParser.cpp
#include "CommandParser.h"
#include "CommandRegistry.h"
#include <QRegularExpression>
#include <QJsonArray>
#include <QDebug>

/* Преобразование объекта с path/content в команду */
static QByteArray convertToObjectCommand(const QByteArray& rawObj);

/* Извлечение команд из текста по ключевым словам */
static QList<QByteArray> findCommandsInText(const QByteArray& rawText);

/* Исправление кавычек вокруг ключей JSON */
static QString fixJsonKeys(const QString& input);

/* Попытка исправить частично неправильный JSON */
static QByteArray attemptJsonFix(const QByteArray& rawJson);

/* Remove code fences and any leading labels like "Assistant:" */
static QByteArray sanitizeRaw(const QByteArray& in)
{
    QByteArray s = in.trimmed();

    // Remove BOM if present
    if (s.startsWith("\xEF\xBB\xBF")) s = s.mid(3);

    // Remove leading "Assistant:" or similar up to first '[' or '{'
    int firstBrace = -1;
    for (int i=0;i<s.size();++i){
        char c = s[i];
        if (c=='[' || c=='{'){ firstBrace = i; break; }
    }
    if (firstBrace>0) s = s.mid(firstBrace);

    // Strip markdown code fences ```json ... ``` or ``` ...
    static QRegularExpression fence(R"(^```[a-zA-Z0-9_-]*\s*([\s\S]*?)\s*```$)");
    auto m = fence.match(QString::fromUtf8(s));
    if (m.hasMatch()) {
        s = m.captured(1).toUtf8();
    }
    return s.trimmed();
}

/* Split concatenated JSON arrays/objects into standalone JSON docs. */
static QList<QByteArray> splitConcatenatedJson(const QByteArray& rawIn)
{
    QByteArray raw = sanitizeRaw(rawIn);
    QList<QByteArray> parts;

    const char* s = raw.constData();
    const int n = raw.size();
    bool inStr=false, esc=false;
    int depth=0;
    int start=-1;
    char openChar=0, closeChar=0;

    for (int i=0;i<n;i++){
        char c = s[i];

        if (inStr){
            if (esc){ esc=false; continue; }
            if (c=='\\'){ esc=true; continue; }
            if (c=='"'){ inStr=false; continue; }
            continue;
        }
        if (c=='"'){ inStr=true; continue; }

        if (depth==0){
            if (c=='[' || c=='{'){
                start = i;
                openChar = c;
                closeChar = (c=='[')?']':'}';
                depth = 1;
                continue;
            } else {
                continue;
            }
        } else {
            if (c==openChar) { depth++; continue; }
            if (c==closeChar){
                depth--;
                if (depth==0 && start>=0){
                    parts << raw.mid(start, i - start + 1);
                    start = -1;
                    openChar = closeChar = 0;
                }
                continue;
            }
        }
    }

    // Если не нашли JSON в стандартном формате, пробуем найти команды по ключевым словам
    if (parts.isEmpty()) {
        // Ищем возможные команды в тексте по ключевым словам
        parts = findCommandsInText(raw);
    }

    return parts;
}

/* Извлечение команд из текста по ключевым словам */
static QList<QByteArray> findCommandsInText(const QByteArray& rawText)
{
    QList<QByteArray> results;
    QString text = QString::fromUtf8(rawText);
    
    // Паттерн для поиска JSON-объектов команд в тексте
    QRegularExpression cmdRegex(R"(\{[^{}]*(?:"command"[^{},]*"[^"]*"[^{}]*|(?:"\w+"\s*:)|(?:\[[^\[\]]*\])|(?:\{[^{}]*\}))*\})", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator it = cmdRegex.globalMatch(text);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QByteArray cmdJson = match.captured().toUtf8().trimmed();
        // Проверяем, содержит ли найденный JSON команду
        if (cmdJson.contains("command") || cmdJson.contains("action")) {
            results << cmdJson;
        } else {
            // Если это объект без "command" или "action", проверим, может ли он быть командой
            QByteArray convertedCmd = convertToObjectCommand(cmdJson);
            if (!convertedCmd.isEmpty()) {
                results << convertedCmd;
            }
        }
    }

    // Если не найдено команд по паттерну, пробуем другие подходы
    if (results.isEmpty()) {
        // Попробуем найти команды в более простом виде
        QRegularExpression simpleCmdRegex(R"(\[([^\[\]]*)\])", QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator simpleIt = simpleCmdRegex.globalMatch(text);
        while (simpleIt.hasNext()) {
            QRegularExpressionMatch match = simpleIt.next();
            QString arrContent = match.captured(1);
            // Ищем в массиве объекты команд
            QRegularExpression objRegex(R"(\{[^}]*\})", QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatchIterator objIt = objRegex.globalMatch(arrContent);
            while (objIt.hasNext()) {
                QRegularExpressionMatch objMatch = objIt.next();
                QByteArray objJson = objMatch.captured().toUtf8().trimmed();
                if (objJson.contains("command") || objJson.contains("action")) {
                    results << objJson;
                } else {
                    // Если это объект без "command" или "action", проверим, может ли он быть командой
                    QByteArray convertedCmd = convertToObjectCommand(objJson);
                    if (!convertedCmd.isEmpty()) {
                        results << convertedCmd;
                    }
                }
            }
        }
    }

    return results;
}

/* Преобразование объекта с path/content в команду CREATE_FILE */
static QByteArray convertToObjectCommand(const QByteArray& rawObj)
{
    QString objStr = QString::fromUtf8(rawObj);
    
    // Проверим, содержит ли объект поля path и content (или file_path и content)
    if ((objStr.contains("path", Qt::CaseInsensitive) || objStr.contains("file_path", Qt::CaseInsensitive)) && objStr.contains("content", Qt::CaseInsensitive)) {
        // Это может быть команда CREATE_FILE
        QJsonObject cmdObj;
        cmdObj["command"] = "CREATE_FILE";
        
        // Пытаемся извлечь путь и содержимое
        QRegularExpression pathRegex(R"((?:\"path\"|path|\"file_path\"|file_path)\s*:\s*\"([^\"]*)\"))");
        QRegularExpressionMatch pathMatch = pathRegex.match(objStr);
        if (pathMatch.hasMatch()) {
            cmdObj["path"] = pathMatch.captured(1);
        }
        
        QRegularExpression contentRegex(R"((?:\"content\"|content)\s*:\s*\"([^\"]*)\"))");
        QRegularExpressionMatch contentMatch = contentRegex.match(objStr);
        if (contentMatch.hasMatch()) {
            cmdObj["content"] = contentMatch.captured(1);
        }
        
        QJsonDocument doc(cmdObj);
        return doc.toJson(QJsonDocument::Compact);
    }
    
    return QByteArray();
}

/* Flatten any combination of arrays/objects into a single array of objects */
static QJsonArray toCommandArray(const QJsonDocument& doc)
{
    QJsonArray out;
    if (doc.isArray()){
        for (const QJsonValue& v : doc.array()){
            if (v.isObject()) {
                out.append(v.toObject());
            }
            else if (v.isArray()){
                // Handle nested arrays recursively
                for (const QJsonValue& vv : v.toArray()) {
                    if (vv.isObject()) {
                        out.append(vv.toObject());
                    }
                    else if (vv.isArray()) {
                        // Handle deeper nesting if needed
                        QJsonArray nestedArr = toCommandArray(QJsonDocument(vv.toArray()));
                        for (const QJsonValue& vvv : nestedArr) {
                            if (vvv.isObject()) {
                                out.append(vvv.toObject());
                            }
                        }
                    }
                }
            }
        }
    } else if (doc.isObject()){
        out.append(doc.object());
    }
    return out;
}

QVector<Command*> CommandParser::parse(const QByteArray& jsonRaw)
{
    QVector<Command*> res;

    QList<QByteArray> docs = splitConcatenatedJson(jsonRaw);
    
    // Если не нашли ничего через splitConcatenatedJson, пробуем другие методы
    if (docs.isEmpty()){
        // Try simple parse in case it's a clean array
        QJsonParseError err;
        QJsonDocument d = QJsonDocument::fromJson(sanitizeRaw(jsonRaw), &err);
        if (err.error == QJsonParseError::NoError){
            docs << sanitizeRaw(jsonRaw);
        }
    }

    // Попытаемся использовать ещё более устойчивый подход - извлечь команды по ключевым словам
    if (docs.isEmpty()) {
        // Ищем возможные команды в тексте, даже если JSON частично неправильный
        docs = findCommandsInText(sanitizeRaw(jsonRaw));
    }

    auto addFromDoc = [&](const QJsonDocument& d){
        QJsonArray arr = toCommandArray(d);
        for (const QJsonValue& v : arr){
            if (!v.isObject()) continue;
            Command* cmd = CommandRegistry::create(v.toObject());
            if (cmd) res.push_back(cmd);
        }
    };

    for (const QByteArray& p : docs){
        QJsonParseError err;
        // Пробуем разобрать каждый найденный фрагмент как JSON
        QJsonDocument d = QJsonDocument::fromJson(p, &err);
        if (err.error != QJsonParseError::NoError){
            // Если не получилось разобрать как JSON, пробуем исправить возможные проблемы
            QByteArray fixedJson = attemptJsonFix(p);
            QJsonDocument fixedDoc = QJsonDocument::fromJson(fixedJson, &err);
            if (err.error == QJsonParseError::NoError) {
                d = fixedDoc;
            } else {
                qWarning() << "CommandParser: skip invalid JSON part:" << err.errorString() << "Content:" << QString::fromUtf8(p).toLocal8Bit();
                continue;
            }
        }
        addFromDoc(d);
    }

    return res;
}

/* Попытка исправить частично неправильный JSON */
static QByteArray attemptJsonFix(const QByteArray& rawJson)
{
    QString jsonStr = QString::fromUtf8(rawJson);
    
    // Удаляем лишние запятые перед закрывающими скобками или скобками массива
    jsonStr.replace(QRegularExpression(R"(\,\s*\})"), "}");
    jsonStr.replace(QRegularExpression(R"(\,\s*\])"), "]");
    
    // Исправляем кавычки вокруг ключей JSON: исправляем {key: } на {"key": }
    jsonStr = fixJsonKeys(jsonStr);
    
    // Проверяем, есть ли незавершённые объекты/массивы и пытаемся их завершить
    // Подсчитываем количество открывающих и закрывающих скобок
    // Используем более простой подход для версии Qt 5.12.12
    int openBraces = jsonStr.count('{');
    int closeBraces = jsonStr.count('}');
    int openBrackets = jsonStr.count('[');
    int closeBrackets = jsonStr.count(']');

    // Если есть незакрытые фигурные скобки, добавляем соответствующее количество закрывающих
    if (openBraces > closeBraces) {
        for (int i = 0; i < openBraces - closeBraces; ++i) {
            jsonStr.append("}");
        }
    }

    // Если есть незакрытые квадратные скобки, добавляем соответствующее количество закрывающих
    if (openBrackets > closeBrackets) {
        for (int i = 0; i < openBrackets - closeBrackets; ++i) {
            jsonStr.append("]");
        }
    }

    // Удаляем лишние запятые перед закрывающими скобками (еще раз после добавления закрывающих)
    jsonStr.replace(QRegularExpression(R"(\,\s*(\}|\]))"), "\\1");

    // Проверяем, не начинается ли строка с лишней запятой или другого недопустимого символа
    jsonStr = jsonStr.trimmed();
    if (jsonStr.startsWith(',')) {
        jsonStr = jsonStr.mid(1).trimmed();
    }

    return jsonStr.toUtf8();
}

/* Исправление кавычек вокруг ключей JSON */
static QString fixJsonKeys(const QString& input)
{
    QString result = input;
    
    // Используем более сложный регулярное выражение, чтобы различать ключи и значения
    // Находим слова, которые идут перед двоеточием и не обрамлены кавычками
    QRegularExpression keyPattern(R"((\s*)([a-zA-Z_][a-zA-Z0-9_]*)\s*(?=:(\s*["\[{])))");
    QRegularExpressionMatchIterator iter = keyPattern.globalMatch(result);
    
    // Проходим через все совпадения и добавляем двойные кавычки вокруг ключей
    int offset = 0;
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        int pos = match.capturedStart(2); // позиция самого ключа
        int len = match.capturedLength(2); // длина ключа
        
        // Вставляем открывающую кавычку перед ключом
        result.insert(pos + offset, "\"");
        offset++;
        // Вставляем закрывающую кавычку после ключа
        result.insert(pos + offset + len, "\"");
        offset++;
    }
    
    return result;
}

QByteArray CommandParser::toJson(const QVector<Command*>& cmds)
{
    QJsonArray arr;
    for (Command* c : cmds)
        arr.append(c->toJson());
    QJsonDocument doc(arr);
    return doc.toJson(QJsonDocument::Indented);
}
