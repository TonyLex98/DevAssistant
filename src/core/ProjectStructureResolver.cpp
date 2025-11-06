#include "ProjectStructureResolver.h"
#include "MainWindow.h"
#include <QDirIterator>
#include <QMap>

QString ProjectStructureResolver::resolveProjectStructure(MainWindow* ctx)
{
    if (!ctx) {
        return QString("Не удается получить структуру проекта: контекст отсутствует");
    }
    
    QString projectRoot = ctx->projectRootPath();
    if (projectRoot.isEmpty()) {
        return QString("Не удается получить структуру проекта: корневая папка проекта не задана");
    }
    
    // Получаем список файлов из ProjectIndex
    QStringList files = ctx->projectIndex().files();
    
    if (files.isEmpty()) {
        return QString("Проект пуст или не содержит файлов");
    }
    
    return generateProjectTree(projectRoot, files);
}

QString ProjectStructureResolver::generateProjectTree(const QString& projectRoot, const QStringList& files)
{
    // Создаем структуру дерева папок
    QSet<QString> directorySet;
    
    for (const QString& file : files) {
        QFileInfo fileInfo(file);
        QString dirPath = fileInfo.path();
        
        // Добавляем все промежуточные папки
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
    
    // Преобразуем в QMap для сортировки по уровням вложенности
    QMap<int, QStringList> directoriesByLevel;
    for (const QString& dir : directorySet) {
        int level = dir.count('/') + 1;
        if (dir.isEmpty()) level = 0; // Корневая директория
        directoriesByLevel[level].append(dir);
    }
    
    // Сортируем имена папок на каждом уровне
    for (auto& dirs : directoriesByLevel) {
        dirs.sort();
    }
    
    // Строим дерево
    QStringList result;
    result << "Структура проекта (только папки):";
    result << "";
    
    // Выводим директории по уровням
    for (auto it = directoriesByLevel.begin(); it != directoriesByLevel.end(); ++it) {
        int level = it.key();
        const QStringList& dirs = it.value();
        
        for (const QString& dirPath : dirs) {
            // Создаем отступы для визуального представления вложенности
            QString indent = "";
            for (int i = 0; i < level; ++i) {
                indent += "  "; // 2 пробела на каждый уровень
            }
            
            // Получаем имя папки
            QStringList pathParts = dirPath.split('/');
            QString folderName = pathParts.isEmpty() ? "" : pathParts.last();
            if (folderName.isEmpty() && level == 0) {
                folderName = QFileInfo(projectRoot).fileName(); // Имя корневой папки проекта
            }
            
            result << QString("%1%2/").arg(indent).arg(folderName);
        }
    }
    
    return result.join('\n');
}