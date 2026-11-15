#pragma once
#include <QString>
#include <QByteArray>
#include <QList>

struct Change {
    enum Type { CreateFile, ReplaceInFile, DeleteFile, RenameFile, CreateDir, DeleteDir } type;
    QString targetPath;         // absolute
    QByteArray newContent;      // for create/replace
    QString aux;                // old path for rename
    QByteArray prevContent;     // for undo
    bool recursive = false;     // for DeleteDir
    bool wasNoop = false;       // for cases where apply does nothing
    bool existedBefore = false; // for ReplaceInFile: undo deletes a file that did not exist

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
    static Change createDir(const QString& abs) {
        Change c; c.type=CreateDir; c.targetPath=abs; return c;
    }
    static Change deleteDir(const QString& abs, bool recursive=false) {
        Change c; c.type=DeleteDir; c.targetPath=abs; c.recursive = recursive; return c;
    }
};

// Applies file changes in batches and keeps an undo history.
//
// A batch is all or nothing: if one change fails, the ones already made are rolled back and
// the history is left as it was. Each successful batch becomes one undo step; up to
// kMaxUndoSteps steps are kept.
class ChangeApplier {
public:
    static constexpr int kMaxUndoSteps = 50;

    bool apply(QList<Change>& changes, QString* errorOut=nullptr);
    // Reverts the most recent batch.
    bool undo(QString* errorOut=nullptr);
    bool canUndo() const { return !m_history.isEmpty(); }
    int undoSteps() const { return m_history.size(); }

private:
    static bool applyOne(Change& c, QString* errorOut);
    static bool revertOne(const Change& c, QString* errorOut);

    QList<QList<Change>> m_history;
};
