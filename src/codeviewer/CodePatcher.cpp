#include "CodePatcher.h"
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
    QFile f(filePath);
    if(!f.open(QIODevice::ReadOnly|QIODevice::Text)) return false;
    QString s=QString::fromUtf8(f.readAll()); f.close();
    QString out = s; out.replace(re, replacement);
    if(out==s) return false;
    QString bak = stashFile(filePath); Q_UNUSED(bak);
    if(!f.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate)) return false;
    QTextStream ts(&f); ts.setCodec("UTF-8"); ts<<out; f.close();
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
    QFile f(clazz.loc.file); if(!f.open(QIODevice::ReadOnly|QIODevice::Text)) return false; QString txt=QString::fromUtf8(f.readAll()); f.close();
    QString paramStr; for(int i=0;i<params.size();++i){ if(i) paramStr+=", "; paramStr+=params[i].type+" "+params[i].name; }
    QString retDefault = (retType=="void") ? "" : ("    return "+retType+"{};\n");
    QString stub = QString("\n\n%1 %2::%3(%4)\n{\n    // TODO: implement\n%5}\n").arg(retType, clazz.displayName, name, paramStr, retDefault);
    QString bak = stashFile(clazz.loc.file); Q_UNUSED(bak);
    if(!f.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Append)) return false; QTextStream ts(&f); ts.setCodec("UTF-8"); ts<<stub; f.close(); return true;
}
