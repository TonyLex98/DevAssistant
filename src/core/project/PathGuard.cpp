#include "core/project/PathGuard.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace {

// Canonical form of `absPath`: the deepest existing ancestor is canonicalized (resolving
// symlinks), and the not-yet-existing tail is appended to it unchanged.
QString canonicalized(const QString& absPath)
{
    QString existing = QDir::cleanPath(absPath);
    QString tail;
    while (!existing.isEmpty() && !QFileInfo::exists(existing)) {
        const int slash = existing.lastIndexOf(QLatin1Char('/'));
        if (slash <= 0)
            return QDir::cleanPath(absPath);
        tail = existing.mid(slash) + tail;
        existing = existing.left(slash);
    }
    const QString canonical = QFileInfo(existing).canonicalFilePath();
    return QDir::cleanPath((canonical.isEmpty() ? existing : canonical) + tail);
}

Qt::CaseSensitivity pathCase()
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

} // namespace

namespace PathGuard {

bool isInside(const QString& projectRoot, const QString& absPath)
{
    if (projectRoot.trimmed().isEmpty() || absPath.trimmed().isEmpty())
        return false;
    const QString root = canonicalized(QDir(projectRoot).absolutePath());
    const QString path = canonicalized(absPath);
    if (path.compare(root, pathCase()) == 0)
        return true;
    const QString prefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
    return path.startsWith(prefix, pathCase());
}

bool resolveInside(const QString& projectRoot, const QString& path,
                   QString* absOut, QString* errorOut)
{
    if (projectRoot.trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("PathGuard", "No project is open.");
        return false;
    }
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("PathGuard", "The path is empty.");
        return false;
    }

    const QString abs = QFileInfo(trimmed).isAbsolute()
            ? QDir::cleanPath(QDir::fromNativeSeparators(trimmed))
            : QDir::cleanPath(QDir(projectRoot).absoluteFilePath(trimmed));

    if (!isInside(projectRoot, abs)) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("PathGuard",
                    "%1 is outside the project folder %2.")
                    .arg(QDir::toNativeSeparators(abs), QDir::toNativeSeparators(projectRoot));
        return false;
    }
    if (absOut)
        *absOut = abs;
    return true;
}

} // namespace PathGuard
