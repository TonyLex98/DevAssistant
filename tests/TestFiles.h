#pragma once
// Small file helpers shared by the tests.

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace TestFiles {

// Writes `data` to `path` as is (binary), creating parent folders.
inline bool write(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(data) == data.size();
}

// Reads `path` as is (binary); an empty array if it cannot be opened.
inline QByteArray read(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

} // namespace TestFiles
