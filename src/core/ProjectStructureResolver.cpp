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
    QMap<QString, QStringList> directoryMap;
    
    for (const QString& file : files) {
        QFileInfo fileInfo(file);
        QString dirPath = fileInfo.path();
        if (dirPath == ".") {
            dirPath = ""; // Корневой уровень
        }
        directoryMap[dirPath].append(fileInfo.fileName());
    }
    
    // Сортируем содержимое каждой директории для упорядоченного вывода
    QMap<QString, QStringList> sortedDirectoryMap;
    for (auto it = directoryMap.begin(); it != directoryMap.end(); ++it) {
        QStringList items = it.value();
        items.sort(Qt::CaseSensitive);
        sortedDirectoryMap[it.key()] = items;
    }
    
    // Строим дерево
    QStringList result;
    result << "Структура проекта:";
    result << "";
    
    // Сначала выводим корневую директорию
    if (sortedDirectoryMap.contains("")) {
        const QStringList& rootFiles = sortedDirectoryMap[""];
        for (const QString& fileName : rootFiles) {
            result << fileName;
        }
    }
    
    // Затем выводим остальные директории
    for (auto it = sortedDirectoryMap.begin(); it != sortedDirectoryMap.end(); ++it) {
        QString dirPath = it.key();
        if (dirPath.isEmpty()) continue; // Пропускаем корневой уровень, он уже обработан
        
        // Подсчитываем уровень вложенности
        QStringList pathParts = dirPath.split('/');
        int level = pathParts.size();
        
        // Создаем отступы для визуального представления вложенности
        QString indent = "";
        for (int i = 0; i < level - 1; ++i) {
            indent += "  "; // 2 пробела на каждый уровень
        }
        
        // Добавляем папку с отступом
        QString folderName = pathParts.last(); // Имя последней папки в пути
        result << QString("%1%2/").arg(indent).arg(folderName);
        
        // Добавляем файлы в этой папке с дополнительным отступом
        const QStringList& dirFiles = it.value();
        for (const QString& fileName : dirFiles) {
            result << QString("%1  %2").arg(indent).arg(fileName);
        }
    }
    
    return result.join('\n');
}