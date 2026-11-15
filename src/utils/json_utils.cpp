#include "json_utils.h"
#include <QFileInfo>
#include <QDir>

QList<QByteArray> splitConcatenatedJson_fromText(const QString& text){
    QByteArray raw = text.toUtf8();
    QList<QByteArray> parts;
    const char* s = raw.constData();
    int n = raw.size();
    bool in_str=false, esc=false;
    int depth=0, start=-1;
    for (int i=0;i<n;i++){
        char c = s[i];
        if (in_str){
            if (esc){ esc=false; continue; }
            if (c=='\\') { esc=true; continue; }
            if (c=='"') { in_str=false; continue; }
            continue;
        }
        if (c=='"'){ in_str=true; continue; }
        if (c=='{' || c=='['){
            if (depth==0) start=i;
            depth++;
        } else if (c=='}' || c==']'){
            if (depth == 0)
                continue; // a stray closing bracket in prose before the JSON
            depth--;
            if (depth==0 && start!=-1){
                parts.append(raw.mid(start, i-start+1));
                start=-1;
            }
        }
    }
    return parts;
}

QString normalizeContextPath(const QString& path, const QString& projectRoot)
{
    if (path.trimmed().isEmpty())
        return QString();
    QFileInfo info(path);
    if (info.isAbsolute())
        return QDir::cleanPath(info.absoluteFilePath());
    return QDir(projectRoot).absoluteFilePath(path);
}

QString displayContextPath(const QString& normalizedPath, const QString& projectRoot)
{
    QDir root(projectRoot);
    QString rel = root.relativeFilePath(normalizedPath);
    if (!rel.startsWith(".."))
        return QDir::toNativeSeparators(rel);
    return QDir::toNativeSeparators(normalizedPath);
}
