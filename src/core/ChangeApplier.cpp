#include "ChangeApplier.h"
#include <QFile>
#include <QDir>

bool ChangeApplier::apply(QList<Change>& changes, QString* errorOut) {
    m_last.clear();
    for (Change c : changes) {
        if (c.type==Change::CreateFile || c.type==Change::ReplaceInFile) {
            QFile f(c.targetPath);
            if (c.type==Change::CreateFile) {
                QDir().mkpath(QFileInfo(c.targetPath).absolutePath());
            } else {
                if (f.exists()) { if (f.open(QIODevice::ReadOnly)) { c.prevContent = f.readAll(); f.close(); } }
            }
            if (!f.open(QIODevice::WriteOnly)) { if (errorOut) *errorOut="Cannot write: "+c.targetPath; return false; }
            f.write(c.newContent); f.close();
        } else if (c.type==Change::DeleteFile) {
            QFile f(c.targetPath);
            if (f.exists()) {
                if (f.open(QIODevice::ReadOnly)) { c.prevContent = f.readAll(); f.close(); }
                if (!f.remove()) { if (errorOut) *errorOut="Cannot delete: "+c.targetPath; return false; }
            }
        } else if (c.type==Change::RenameFile) {
            QFile f(c.aux);
            if (f.exists()) {
                QFile prev(c.aux);
                if (prev.open(QIODevice::ReadOnly)) { c.prevContent = prev.readAll(); prev.close(); }
            }
            QDir().mkpath(QFileInfo(c.targetPath).absolutePath());
            if (!QFile::rename(c.aux, c.targetPath)) { if (errorOut) *errorOut="Cannot rename: "+c.aux+" -> "+c.targetPath; return false; }
        }
        m_last.push_back(c);
    }
    return true;
}

bool ChangeApplier::undo(QString* errorOut) {
    for (int i=m_last.size()-1;i>=0;--i) {
        const Change& c = m_last[i];
        if (c.type==Change::CreateFile) {
            QFile::remove(c.targetPath);
        } else if (c.type==Change::ReplaceInFile) {
            QFile f(c.targetPath);
            if (!f.open(QIODevice::WriteOnly)) { if (errorOut) *errorOut="Cannot undo write: "+c.targetPath; return false; }
            f.write(c.prevContent); f.close();
        } else if (c.type==Change::DeleteFile) {
            QFile f(c.targetPath);
            QDir().mkpath(QFileInfo(c.targetPath).absolutePath());
            if (!f.open(QIODevice::WriteOnly)) { if (errorOut) *errorOut="Cannot undo delete: "+c.targetPath; return false; }
            f.write(c.prevContent); f.close();
        } else if (c.type==Change::RenameFile) {
            if (!QFile::rename(c.targetPath, c.aux)) { if (errorOut) *errorOut="Cannot undo rename: "+c.targetPath+" -> "+c.aux; return false; }
        }
    }
    m_last.clear();
    return true;
}
