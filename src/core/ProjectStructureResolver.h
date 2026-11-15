#pragma once
#include <QString>
#include <QStringList>
#include <QDir>

class MainWindow;  // forward declaration

class ProjectStructureResolver
{
public:
    /* Builds a tree view of the project structure */
    static QString resolveProjectStructure(MainWindow* ctx);
    
private:
    static QString generateProjectTree(const QString& projectRoot, const QStringList& files);
};