#include "CodePatcher.h"
#include "../utils/TextFileLines.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>
#include <QRegularExpression>

CodePatcher::CodePatcher(QObject* parent): QObject(parent){}

QString CodePatcher::stashDirForFile(const QString &filePath) const{
    QFileInfo fi(filePath); QDir d(fi.dir());
    QString stash = d.absolutePath()+"/.graph_stash"; QDir().mkpath(stash); return stash;
}

QString CodePatcher::stashFile(const QString &filePath){
    QFile in(filePath); if(!in.open(QIODevice::ReadOnly)) return QString(); QByteArray data=in.readAll(); in.close();
    QString stashDir = stashDirForFile(filePath);
    QString name = QFileInfo(filePath).fileName()+"_"+QString::number(QDateTime::currentMSecsSinceEpoch())+".bak";
    QString outPath = stashDir+"/"+name; QFile out(outPath);
    if(out.open(QIODevice::WriteOnly)){ out.write(data); out.close(); return outPath; } return QString();
}

bool CodePatcher::restoreFromStash(const QString &filePath){
    QString stashDir = stashDirForFile(filePath);
    QDir d(stashDir);
    // Only this file's backups (stashFile names them "<file name>_<msecs>.bak"); the folder
    // also holds backups of other files.
    auto list = d.entryInfoList(QStringList() << (QFileInfo(filePath).fileName() + "_*.bak"),
                                QDir::Files, QDir::Time|QDir::Reversed);
    if(list.isEmpty()) return false;
    QFileInfo last = list.last();
    QFile in(last.absoluteFilePath()); if(!in.open(QIODevice::ReadOnly)) return false;
    QByteArray data=in.readAll(); in.close();
    QFile out(filePath); if(!out.open(QIODevice::WriteOnly)) return false;
    out.write(data); out.close(); return true;
}

bool CodePatcher::replaceInFile(const QString &filePath, const QRegExp &re, const QString &replacement){
    // Binary I/O through TextFileLines: line endings, BOM and the final newline stay as they were.
    QFile f(filePath);
    if(!f.open(QIODevice::ReadOnly)) return false;
    TextFileLines doc = TextFileLines::fromBytes(f.readAll()); f.close();
    const QString s = doc.lines.join(QLatin1Char('\n'));
    QString out = s; out.replace(re, replacement);
    if(out==s) return false;
    QString bak = stashFile(filePath); Q_UNUSED(bak);
    doc.lines = out.split(QLatin1Char('\n'));
    if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate)) return false;
    f.write(doc.toBytes()); f.close();
    return true;
}

bool CodePatcher::renameMethod(const NodeData &method, const QString &newName){
    if(method.loc.file.isEmpty()) return false;
    QString file = method.loc.file;
    // naive: rename "name(" -> "newName("
    QRegExp re(QString("\\b%1\\s*\\(").arg(QRegExp::escape(method.canonicalName)));
    re.setMinimal(true);
    return replaceInFile(file, re, newName+"(");
}

bool CodePatcher::addMethodStub(const NodeData &clazz, const QString &retType, const QString &name, const QVector<ParamInfo>& params){
    if(clazz.loc.file.isEmpty()) return false;
    QFile f(clazz.loc.file); if(!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray eol = TextFileLines::fromBytes(f.readAll()).eol; f.close();
    QString paramStr; for(int i=0;i<params.size();++i){ if(i) paramStr+=", "; paramStr+=params[i].type+" "+params[i].name; }
    QString retDefault = (retType=="void") ? "" : ("    return "+retType+"{};\n");
    QString stub = QString("\n\n%1 %2::%3(%4)\n{\n    // TODO: implement\n%5}\n").arg(retType, clazz.displayName, name, paramStr, retDefault);
    QString bak = stashFile(clazz.loc.file); Q_UNUSED(bak);
    // Appended in the file's own line endings.
    if(!f.open(QIODevice::WriteOnly|QIODevice::Append)) return false;
    f.write(stub.toUtf8().replace("\n", eol)); f.close(); return true;
}
