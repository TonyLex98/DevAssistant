// CommandParser.cpp
#include "CommandParser.h"
#include "CommandRegistry.h"
#include <QRegularExpression>

static QList<QByteArray> splitConcatenatedJson(const QByteArray& raw)
{
    QList<QByteArray> parts;
    const char* s = raw.constData();
    int n = raw.size();
    bool in_str = false, esc = false;
    bool in_cmt = false, in_cpp_cmt = false; // C‑style и C++‑style комментарии
    int depth = 0, start = -1;
    for (int i=0;i<n;i++){
        char c = s[i];
        // 1) Строки (JSON‑строки)
        if (in_str){
            if (esc){ esc = false; continue; }
            if (c=='\\') { esc = true; continue; }
            if (c=='"') { in_str = false; continue; }
            continue;
        }
        // 2) C‑style комментарий /* ... */
        if (in_cmt){
            if (c=='*' && i+1<n && s[i+1]=='/'){
                in_cmt = false;
                ++i; continue;
            }
            continue;
        }
        // 3) C++‑style комментарий // ...
        if (in_cpp_cmt){
            if (c=='\n' || c=='\r'){ in_cpp_cmt = false; }
            continue;
        }
        // 4) Начало комментария
        if (c=='/' && i+1<n){
            if (s[i+1]=='*'){ in_cmt = true; ++i; continue; }
            if (s[i+1]=='/'){ in_cpp_cmt = true; ++i; continue; }
        }
        // 5) Строка начинается
        if (c=='"'){ in_str = true; continue; }
        // 6) Фигурные скобки
        if (c=='{' || c=='['){
            if (depth==0) start = i;
            depth++;
        } else if (c=='}' || c==']'){
            depth--;
            if (depth==0 && start!=-1){
                parts.append(raw.mid(start, i-start+1));
                start = -1;
            }
        }
    }
    return parts;
}



QVector<Command*> CommandParser::parse(const QByteArray& json)
{
    QVector<Command*> res;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    auto addFromDoc = [&](const QJsonDocument& d){
        if (d.isArray()){
            for (const QJsonValue& v : d.array()){
                if (v.isObject()){
                    if (Command* c = CommandRegistry::create(v.toObject())) res.append(c);
                }
            }
        } else if (d.isObject()){
            if (Command* c = CommandRegistry::create(d.object())) res.append(c);
        }
    };

    if (err.error == QJsonParseError::NoError) {
        addFromDoc(doc);
        return res;
    }

    // Try to split concatenated or embedded JSON
    QList<QByteArray> parts = splitConcatenatedJson(json);
    for (const QByteArray& p : parts){
        QJsonDocument d = QJsonDocument::fromJson(p);
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
