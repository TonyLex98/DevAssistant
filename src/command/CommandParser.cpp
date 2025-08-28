// CommandParser.cpp
#include "CommandParser.h"
#include "CommandRegistry.h"
#include <QRegularExpression>
#include <QJsonArray>
#include <QDebug>

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
    return parts;
}

/* Flatten any combination of arrays/objects into a single array of objects */
static QJsonArray toCommandArray(const QJsonDocument& doc)
{
    QJsonArray out;
    if (doc.isArray()){
        for (const QJsonValue& v : doc.array()){
            if (v.isObject()) out.append(v.toObject());
            else if (v.isArray()){
                for (const QJsonValue& vv : v.toArray())
                    if (vv.isObject()) out.append(vv.toObject());
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
    if (docs.isEmpty()){
        // Try simple parse in case it's a clean array
        QJsonParseError err;
        QJsonDocument d = QJsonDocument::fromJson(sanitizeRaw(jsonRaw), &err);
        if (err.error == QJsonParseError::NoError){
            docs << sanitizeRaw(jsonRaw);
        }
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
            qWarning() << "CommandParser: skip invalid JSON part:" << err.errorString();
            continue;
        }
        addFromDoc(d);
    }

    return res;
}

QByteArray CommandParser::toJson(const QVector<Command*>& cmds)
{
    QJsonArray arr;
    for (Command* c : cmds)
        arr.append(c->toJson());
    QJsonDocument doc(arr);
    return doc.toJson(QJsonDocument::Indented);
}
