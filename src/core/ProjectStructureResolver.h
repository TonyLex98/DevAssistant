#pragma once
#include <QString>
#include <QStringList>
#include <QDir>

class MainWindow;  // forward declaration

class ProjectStructureResolver
{
public:
    /* Генерирует древовидное представление структуры проекта */
    static QString resolveProjectStructure(MainWindow* ctx);
    
private:
    /* Вспомогательная функция для формирования дерева */
    static QString generateProjectTree(const QString& projectRoot, const QStringList& files);
};