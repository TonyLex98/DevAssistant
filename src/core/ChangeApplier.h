#pragma once
#include <QString>
#include <QByteArray>
#include <QList>

struct Change {
    enum Type { CreateFile, ReplaceInFile, DeleteFile, RenameFile } type;
    QString targetPath;         // absolute
    QByteArray newContent;      // for create/replace
    QString aux;                // old path for rename
    QByteArray prevContent;     // for undo

    static Change createFile(const QString& abs, const QByteArray& content) {
        Change c; c.type=CreateFile; c.targetPath=abs; c.newContent=content; return c;
    }
    static Change replaceInFile(const QString& abs, const QByteArray& content){
        Change c; c.type=ReplaceInFile; c.targetPath=abs; c.newContent=content; return c;
    }
    static Change deleteFile(const QString& abs){
        Change c; c.type=DeleteFile; c.targetPath=abs; return c;
    }
    static Change renameFile(const QString& fromAbs, const QString& toAbs){
        Change c; c.type=RenameFile; c.targetPath=toAbs; c.aux=fromAbs; return c;
    }
};

class ChangeApplier {
public:
    bool apply(QList<Change>& changes, QString* errorOut=nullptr);
    bool undo(QString* errorOut=nullptr);

private:
    QList<Change> m_last;
};
