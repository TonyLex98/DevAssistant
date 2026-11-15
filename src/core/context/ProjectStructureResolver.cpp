#include "core/context/ProjectStructureResolver.h"
#include "ui/MainWindow.h"
#include <QDirIterator>
#include <QMap>

QString ProjectStructureResolver::resolveProjectStructure(MainWindow* ctx)
{
    if (!ctx) {
        return QString("Cannot get the project structure: no context");
    }
    
    QString projectRoot = ctx->projectRootPath();
    if (projectRoot.isEmpty()) {
        return QString("Cannot get the project structure: the project root folder is not set");
    }
    
    // File list from ProjectIndex
    QStringList files = ctx->projectIndex().files();
    
    if (files.isEmpty()) {
        return QString("The project is empty or contains no files");
    }
    
    return generateProjectTree(projectRoot, files);
}

QString ProjectStructureResolver::generateProjectTree(const QString& projectRoot, const QStringList& files)
{
    // Collect the folder tree
    QSet<QString> directorySet;
    
    for (const QString& file : files) {
        QFileInfo fileInfo(file);
        QString dirPath = fileInfo.path();
        
        // Add every intermediate folder
        QStringList pathParts = dirPath.split('/');
        QString currentPath = "";
        for (int i = 0; i < pathParts.size(); ++i) {
            if (!currentPath.isEmpty()) {
                currentPath += "/";
            }
            currentPath += pathParts[i];
            directorySet.insert(currentPath);
        }
    }
    
    // Group by nesting level
    QMap<int, QStringList> directoriesByLevel;
    for (const QString& dir : directorySet) {
        int level = dir.count('/') + 1;
        if (dir.isEmpty()) level = 0; // root directory
        directoriesByLevel[level].append(dir);
    }
    
    // Sort folder names within each level
    for (auto& dirs : directoriesByLevel) {
        dirs.sort();
    }
    
    QStringList result;
    result << "Project structure (folders only):";
    result << "";
    
    // Output folders level by level
    for (auto it = directoriesByLevel.begin(); it != directoriesByLevel.end(); ++it) {
        int level = it.key();
        const QStringList& dirs = it.value();
        
        for (const QString& dirPath : dirs) {
            // Indent 2 spaces per nesting level
            QString indent = "";
            for (int i = 0; i < level; ++i) {
                indent += "  ";
            }

            QStringList pathParts = dirPath.split('/');
            QString folderName = pathParts.isEmpty() ? "" : pathParts.last();
            if (folderName.isEmpty() && level == 0) {
                folderName = QFileInfo(projectRoot).fileName(); // project root folder name
            }
            
            result << QString("%1%2/").arg(indent).arg(folderName);
        }
    }
    
    return result.join('\n');
}