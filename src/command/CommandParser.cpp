// CommandParser.cpp
#include "CommandParser.h"
#include "CommandRegistry.h"
#include <QRegularExpression>
#include <QJsonArray>
#include <QSet>
#include "utils/Logging.h"

/* Turns an object with path/content into a command */
static QByteArray convertToObjectCommand(const QByteArray& rawObj);

/* Extracts commands from free text by keywords */
static QList<QByteArray> findCommandsInText(const QByteArray& rawText);

/* Quotes bare JSON keys */
static QString fixJsonKeys(const QString& input);

/* Tries to repair partially broken JSON */
static QByteArray attemptJsonFix(const QByteArray& rawJson);

/* Fallback parser for DSL-like lines: COMMAND key=value, key2="value" */
static QList<QByteArray> findDslCommandsInText(const QByteArray& rawText);

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

    // No well-formed JSON found: look for commands by keywords.
    if (parts.isEmpty()) {
        parts = findCommandsInText(raw);
    }

    if (parts.isEmpty()) {
        // Last-resort parser for weak-model DSL output:
        // OPEN_FILE path=src/main.cpp
        parts = findDslCommandsInText(raw);
    }

    return parts;
}

/* Extracts commands from free text by keywords */
static QList<QByteArray> findCommandsInText(const QByteArray& rawText)
{
    QList<QByteArray> results;
    QString text = QString::fromUtf8(rawText);

    // Matches command-like JSON objects inside the text
    QRegularExpression cmdRegex(R"(\{[^{}]*(?:"command"[^{},]*"[^"]*"[^{}]*|(?:"\w+"\s*:)|(?:\[[^\[\]]*\])|(?:\{[^{}]*\}))*\})", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator it = cmdRegex.globalMatch(text);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QByteArray cmdJson = match.captured().toUtf8().trimmed();
        if (cmdJson.contains("command") || cmdJson.contains("action")) {
            results << cmdJson;
        } else {
            // No "command"/"action": check whether it can still be a command.
            QByteArray convertedCmd = convertToObjectCommand(cmdJson);
            if (!convertedCmd.isEmpty()) {
                results << convertedCmd;
            }
        }
    }

    // Nothing matched: try a simpler form (objects inside a [...] array).
    if (results.isEmpty()) {
        QRegularExpression simpleCmdRegex(R"(\[([^\[\]]*)\])", QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator simpleIt = simpleCmdRegex.globalMatch(text);
        while (simpleIt.hasNext()) {
            QRegularExpressionMatch match = simpleIt.next();
            QString arrContent = match.captured(1);
            QRegularExpression objRegex(R"(\{[^}]*\})", QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatchIterator objIt = objRegex.globalMatch(arrContent);
            while (objIt.hasNext()) {
                QRegularExpressionMatch objMatch = objIt.next();
                QByteArray objJson = objMatch.captured().toUtf8().trimmed();
                if (objJson.contains("command") || objJson.contains("action")) {
                    results << objJson;
                } else {
                    // No "command"/"action": check whether it can still be a command.
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

static QString stripWrappingQuotes(QString s)
{
    s = s.trimmed();
    if (s.size() >= 2) {
        const QChar a = s.front();
        const QChar b = s.back();
        if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
            s = s.mid(1, s.size() - 2);
        }
    }
    return s.trimmed();
}

static QJsonObject parseDslParams(const QString& tail)
{
    QJsonObject params;
    const QString trimmed = tail.trimmed();
    if (trimmed.isEmpty())
        return params;

    // key=value key2="value 2" key3='value 3'
    QRegularExpression kvRe(R"(([A-Za-z_][A-Za-z0-9_\-]*)\s*=\s*(\"[^\"]*\"|'[^']*'|[^,\s]+))");
    QRegularExpressionMatchIterator it = kvRe.globalMatch(trimmed);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString key = m.captured(1).trimmed();
        const QString value = stripWrappingQuotes(m.captured(2));
        params.insert(key, value);
    }

    if (!params.isEmpty())
        return params;

    // "key: value" form. The key must be a parameter name of at least two characters, so a
    // Windows path such as C:\src\main.cpp is not split at its drive letter.
    static const QRegularExpression keyValueRe(R"(^([A-Za-z_][A-Za-z0-9_]+)\s*:\s*(.+)$)");
    const QRegularExpressionMatch kv = keyValueRe.match(trimmed);
    if (kv.hasMatch()) {
        const QString value = stripWrappingQuotes(kv.captured(2));
        if (!value.isEmpty()) {
            params.insert(kv.captured(1), value);
            return params;
        }
    }

    return params;
}

static QList<QByteArray> findDslCommandsInText(const QByteArray& rawText)
{
    QList<QByteArray> out;
    const QString text = QString::fromUtf8(rawText);
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    // Only an upper-case command name of a known command starts a DSL line
    // ("OPEN_FILE src/main.cpp"). Ordinary prose such as "2. Undo the previous change" or
    // "Open the file" must not turn into commands.
    QRegularExpression lineRe(R"(^\s*(?:[-*•]\s*)?(?:\d+[\.\)]\s*)?([A-Z][A-Z0-9_]{2,})\b\s*(.*)$)");
    QSet<QString> knownCommands;
    for (Command* c : CommandRegistry::all()) {
        knownCommands.insert(c->name());
        delete c;
    }

    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        const QRegularExpressionMatch m = lineRe.match(line);
        if (!m.hasMatch())
            continue;

        QString cmd = m.captured(1).trimmed();
        QString tail = m.captured(2).trimmed();
        if (cmd.isEmpty() || !knownCommands.contains(cmd))
            continue;

        QJsonObject obj;
        obj["command"] = cmd;

        // COMMAND { ... } form
        if (tail.startsWith('{') && tail.endsWith('}')) {
            QJsonParseError err;
            const QJsonDocument d = QJsonDocument::fromJson(tail.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && d.isObject()) {
                obj["params"] = d.object();
            } else {
                QJsonObject params = parseDslParams(tail);
                if (!params.isEmpty())
                    obj["params"] = params;
            }
        } else {
            QJsonObject params = parseDslParams(tail);
            if (!params.isEmpty()) {
                obj["params"] = params;
            } else if (!tail.isEmpty()) {
                // Friendly fallback for commands where a single argument is usually path/query.
                const QString value = stripWrappingQuotes(tail);
                if (!value.isEmpty())
                    obj["params"] = QJsonObject{{"path", value}};
            }
        }

        out << QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    return out;
}

/* Turns an object with path/content into a CREATE_FILE command */
static QByteArray convertToObjectCommand(const QByteArray& rawObj)
{
    QString objStr = QString::fromUtf8(rawObj);

    // An object with path (or file_path) and content can be a CREATE_FILE command.
    if ((objStr.contains("path", Qt::CaseInsensitive) || objStr.contains("file_path", Qt::CaseInsensitive)) && objStr.contains("content", Qt::CaseInsensitive)) {
        QJsonObject cmdObj;
        cmdObj["command"] = "CREATE_FILE";

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
    
    // Nothing found by splitConcatenatedJson: try other methods.
    if (docs.isEmpty()){
        // Try simple parse in case it's a clean array
        QJsonParseError err;
        QJsonDocument d = QJsonDocument::fromJson(sanitizeRaw(jsonRaw), &err);
        if (err.error == QJsonParseError::NoError){
            docs << sanitizeRaw(jsonRaw);
        }
    }

    // More tolerant: extract commands by keywords even from partially broken JSON.
    if (docs.isEmpty()) {
        docs = findCommandsInText(sanitizeRaw(jsonRaw));
    }

    if (docs.isEmpty()) {
        docs = findDslCommandsInText(sanitizeRaw(jsonRaw));
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
        QJsonDocument d = QJsonDocument::fromJson(p, &err);
        if (err.error != QJsonParseError::NoError){
            // Not valid JSON: try to repair it.
            QByteArray fixedJson = attemptJsonFix(p);
            QJsonDocument fixedDoc = QJsonDocument::fromJson(fixedJson, &err);
            if (err.error == QJsonParseError::NoError) {
                d = fixedDoc;
            } else {
                qCWarning(logCmd) << "CommandParser: skip invalid JSON part:" << err.errorString() << "Content:" << QString::fromUtf8(p).toLocal8Bit();
                continue;
            }
        }
        addFromDoc(d);
    }

    return res;
}

/* Tries to repair partially broken JSON */
static QByteArray attemptJsonFix(const QByteArray& rawJson)
{
    QString jsonStr = QString::fromUtf8(rawJson);

    // Normalize smart quotes to ASCII quotes.
    jsonStr.replace(QChar(0x201C), '"');
    jsonStr.replace(QChar(0x201D), '"');
    jsonStr.replace(QChar(0x00AB), '"');
    jsonStr.replace(QChar(0x00BB), '"');
    jsonStr.replace(QChar(0x2018), '\'');
    jsonStr.replace(QChar(0x2019), '\'');
    
    // Remove trailing commas before closing braces/brackets.
    jsonStr.replace(QRegularExpression(R"(\,\s*\})"), "}");
    jsonStr.replace(QRegularExpression(R"(\,\s*\])"), "]");
    
    // Quote bare keys: {key: } -> {"key": }
    jsonStr = fixJsonKeys(jsonStr);

    // Support JSON-like output that uses single quotes.
    jsonStr.replace(QRegularExpression(R"('([A-Za-z_][A-Za-z0-9_]*)'\s*:)"), "\"\\1\":");
    jsonStr.replace(QRegularExpression(R"(:\s*'([^']*)')"), ":\"\\1\"");
    
    // Close unterminated objects/arrays by counting opening and closing brackets.
    int openBraces = jsonStr.count('{');
    int closeBraces = jsonStr.count('}');
    int openBrackets = jsonStr.count('[');
    int closeBrackets = jsonStr.count(']');

    if (openBraces > closeBraces) {
        for (int i = 0; i < openBraces - closeBraces; ++i) {
            jsonStr.append("}");
        }
    }

    if (openBrackets > closeBrackets) {
        for (int i = 0; i < openBrackets - closeBrackets; ++i) {
            jsonStr.append("]");
        }
    }

    // Remove trailing commas again after the closing brackets were added.
    jsonStr.replace(QRegularExpression(R"(\,\s*(\}|\]))"), "\\1");

    // Drop a stray leading comma.
    jsonStr = jsonStr.trimmed();
    if (jsonStr.startsWith(',')) {
        jsonStr = jsonStr.mid(1).trimmed();
    }

    return jsonStr.toUtf8();
}

/* Quotes bare JSON keys */
static QString fixJsonKeys(const QString& input)
{
    // Quotes bare keys ({key: ...} -> {"key": ...}). It walks the text and only looks at
    // words right after '{' or ',' outside string values, so text such as
    // "content": "Result: [ok]" inside a value is never touched.
    QString out;
    out.reserve(input.size() + 16);
    bool inString = false;
    bool escaped = false;
    QChar lastSignificant;
    const int n = input.size();
    int i = 0;
    while (i < n) {
        const QChar c = input.at(i);
        if (inString) {
            out += c;
            if (escaped)
                escaped = false;
            else if (c == QLatin1Char('\\'))
                escaped = true;
            else if (c == QLatin1Char('"')) {
                inString = false;
                lastSignificant = c;
            }
            ++i;
            continue;
        }
        if (c == QLatin1Char('"')) {
            inString = true;
            out += c;
            ++i;
            continue;
        }
        if ((c.isLetter() || c == QLatin1Char('_'))
                && (lastSignificant == QLatin1Char('{') || lastSignificant == QLatin1Char(','))) {
            int end = i;
            while (end < n && (input.at(end).isLetterOrNumber() || input.at(end) == QLatin1Char('_')))
                ++end;
            int colon = end;
            while (colon < n && input.at(colon).isSpace())
                ++colon;
            if (colon < n && input.at(colon) == QLatin1Char(':')) {
                out += QLatin1Char('"') + input.mid(i, end - i) + QLatin1Char('"');
                lastSignificant = QLatin1Char('"');
                i = end;
                continue;
            }
        }
        out += c;
        if (!c.isSpace())
            lastSignificant = c;
        ++i;
    }
    return out;
}

QByteArray CommandParser::toJson(const QVector<Command*>& cmds)
{
    QJsonArray arr;
    for (Command* c : cmds)
        arr.append(c->toJson());
    QJsonDocument doc(arr);
    return doc.toJson(QJsonDocument::Indented);
}
