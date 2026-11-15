#include "ChangeApplier.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>

namespace {

bool readAll(const QString& path, QByteArray* out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    *out = f.readAll();
    return true;
}

bool writeAll(const QString& path, const QByteArray& data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(data) == data.size();
}

} // namespace

bool ChangeApplier::applyOne(Change& c, QString* errorOut)
{
    if (c.targetPath.trimmed().isEmpty()) {
        if (errorOut) *errorOut = "Empty path";
        return false;
    }

    switch (c.type) {
    case Change::CreateDir:
        if (QDir(c.targetPath).exists()) {
            c.wasNoop = true;
            return true;
        }
        if (!QDir().mkpath(c.targetPath)) {
            if (errorOut) *errorOut = "Cannot create dir: " + c.targetPath;
            return false;
        }
        return true;

    case Change::DeleteDir: {
        QDir d(c.targetPath);
        if (!d.exists()) {
            c.wasNoop = true;
            return true;
        }
        const bool ok = c.recursive ? d.removeRecursively() : d.rmdir(c.targetPath);
        if (!ok) {
            if (errorOut) *errorOut = "Cannot delete dir: " + c.targetPath;
            return false;
        }
        return true;
    }

    case Change::CreateFile:
        if (QFileInfo::exists(c.targetPath)) {
            if (errorOut) *errorOut = "File already exists: " + c.targetPath;
            return false;
        }
        QDir().mkpath(QFileInfo(c.targetPath).absolutePath());
        if (!writeAll(c.targetPath, c.newContent)) {
            if (errorOut) *errorOut = "Cannot write: " + c.targetPath;
            return false;
        }
        return true;

    case Change::ReplaceInFile:
        c.existedBefore = QFileInfo::exists(c.targetPath);
        if (c.existedBefore && !readAll(c.targetPath, &c.prevContent)) {
            if (errorOut) *errorOut = "Cannot read: " + c.targetPath;
            return false;
        }
        QDir().mkpath(QFileInfo(c.targetPath).absolutePath());
        if (!writeAll(c.targetPath, c.newContent)) {
            if (errorOut) *errorOut = "Cannot write: " + c.targetPath;
            return false;
        }
        return true;

    case Change::DeleteFile:
        if (!QFileInfo::exists(c.targetPath)) {
            c.wasNoop = true;
            return true;
        }
        if (!readAll(c.targetPath, &c.prevContent) || !QFile::remove(c.targetPath)) {
            if (errorOut) *errorOut = "Cannot delete: " + c.targetPath;
            return false;
        }
        return true;

    case Change::RenameFile:
        if (QFileInfo::exists(c.targetPath)) {
            if (errorOut) *errorOut = "Cannot rename: target exists: " + c.targetPath;
            return false;
        }
        QDir().mkpath(QFileInfo(c.targetPath).absolutePath());
        if (!QFile::rename(c.aux, c.targetPath)) {
            if (errorOut) *errorOut = "Cannot rename: " + c.aux + " -> " + c.targetPath;
            return false;
        }
        return true;
    }
    return false;
}

bool ChangeApplier::revertOne(const Change& c, QString* errorOut)
{
    if (c.wasNoop)
        return true;
    switch (c.type) {
    case Change::CreateDir:
        // Only an empty folder is removed; anything put into it since stays.
        QDir(c.targetPath).rmdir(c.targetPath);
        return true;
    case Change::DeleteDir:
        if (c.recursive) {
            if (errorOut) *errorOut = "Cannot undo recursive dir delete: " + c.targetPath;
            return false;
        }
        QDir().mkpath(c.targetPath);
        return true;
    case Change::CreateFile:
        QFile::remove(c.targetPath);
        return true;
    case Change::ReplaceInFile:
        if (!c.existedBefore) {
            QFile::remove(c.targetPath);
            return true;
        }
        if (!writeAll(c.targetPath, c.prevContent)) {
            if (errorOut) *errorOut = "Cannot undo write: " + c.targetPath;
            return false;
        }
        return true;
    case Change::DeleteFile:
        QDir().mkpath(QFileInfo(c.targetPath).absolutePath());
        if (!writeAll(c.targetPath, c.prevContent)) {
            if (errorOut) *errorOut = "Cannot undo delete: " + c.targetPath;
            return false;
        }
        return true;
    case Change::RenameFile:
        if (!QFile::rename(c.targetPath, c.aux)) {
            if (errorOut) *errorOut = "Cannot undo rename: " + c.targetPath + " -> " + c.aux;
            return false;
        }
        return true;
    }
    return false;
}

bool ChangeApplier::apply(QList<Change>& changes, QString* errorOut)
{
    QList<Change> done;
    for (Change c : changes) {
        if (!applyOne(c, errorOut)) {
            // All or nothing: put back what this batch already changed.
            for (int i = done.size() - 1; i >= 0; --i)
                revertOne(done.at(i), nullptr);
            return false;
        }
        done.push_back(c);
    }
    changes = done;
    if (!done.isEmpty()) {
        m_history.push_back(done);
        while (m_history.size() > kMaxUndoSteps)
            m_history.removeFirst();
    }
    return true;
}

bool ChangeApplier::undo(QString* errorOut)
{
    if (m_history.isEmpty()) {
        if (errorOut) *errorOut = "Nothing to undo";
        return false;
    }
    const QList<Change> batch = m_history.takeLast();
    bool ok = true;
    for (int i = batch.size() - 1; i >= 0; --i) {
        QString err;
        if (!revertOne(batch.at(i), &err)) {
            ok = false;
            if (errorOut) *errorOut = err;
        }
    }
    return ok;
}
