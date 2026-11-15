#pragma once
#include <QString>

// Keeps every path a model command touches inside the open project.
//
// Paths are resolved against the project root, cleaned of "." and "..", and then compared
// using canonical (symlink-free) forms, so neither "../" nor a symlink can lead outside.
// A path that does not exist yet (a file about to be created) is checked through its
// nearest existing parent folder.
namespace PathGuard {

// Resolves `path` (absolute or relative to `projectRoot`) and checks that it stays inside
// the project. On success stores the cleaned absolute path in `absOut` and returns true;
// otherwise stores a reason in `errorOut` and returns false. Fails when no project is open.
bool resolveInside(const QString& projectRoot, const QString& path,
                   QString* absOut, QString* errorOut);

// True if `absPath` is `projectRoot` itself or lies under it.
bool isInside(const QString& projectRoot, const QString& absPath);

} // namespace PathGuard
