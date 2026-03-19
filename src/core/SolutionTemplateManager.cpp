// SolutionTemplateManager.cpp
#include "SolutionTemplateManager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QFileInfo>

static bool copyRecursively(const QString& sourcePath, const QString& destPath)
{
    QFileInfo srcInfo(sourcePath);
    if (!srcInfo.exists())
        return false;

    if (srcInfo.isFile()) {
        QFile::remove(destPath);
        return QFile::copy(sourcePath, destPath);
    }

    QDir srcDir(sourcePath);
    if (!srcDir.exists())
        return false;

    QDir destDir(destPath);
    if (!destDir.exists() && !QDir().mkpath(destPath))
        return false;

    const QFileInfoList entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo& entry : entries) {
        const QString srcChild = entry.absoluteFilePath();
        const QString dstChild = QDir(destPath).filePath(entry.fileName());
        if (!copyRecursively(srcChild, dstChild))
            return false;
    }
    return true;
}

SolutionTemplateManager::SolutionTemplateManager(QObject *parent)
    : QObject(parent)
{
}

void SolutionTemplateManager::setRoot(const QString &rootPath)
{
    m_root = rootPath;
    refresh();
}

void SolutionTemplateManager::refresh()
{
    if (false) qDebug() << "[SolutionTemplateManager] Starting refresh operation, root path:" << m_root;
    m_templates.clear();
    m_templateDirs.clear();

    if (m_root.isEmpty()) {
        if (false) qDebug() << "[SolutionTemplateManager] Root path is empty, returning";
        return;
    }

    QDir dir(m_root);
    if (!dir.exists()) {
        if (false) qDebug() << "[SolutionTemplateManager] Root directory does not exist:" << m_root;
        return;
    }

    // Получаем все поддиректории (это будут папки шаблонов)
    QFileInfoList entryList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (false) qDebug() << "[SolutionTemplateManager] Found" << entryList.size() << "template directories";
    
    for (const QFileInfo& info : entryList) {
        QString templateDirPath = info.absoluteFilePath();
        if (false) qDebug() << "[SolutionTemplateManager] Processing template directory:" << templateDirPath;
        
        // Синхронизируем содержимое папки шаблона с файлом шаблона
        bool syncSuccess = syncTemplateFileWithFolder(templateDirPath);
        if (false) qDebug() << "[SolutionTemplateManager] syncTemplateFileWithFolder result for" << templateDirPath << ":" << syncSuccess;
        
        QString templateJsonPath = templateDirPath + "/template.templ.json";
        if (false) qDebug() << "[SolutionTemplateManager] Attempting to load template file:" << templateJsonPath;
        
        // Загружаем JSON-файл шаблона
        QFile tmplFile(templateJsonPath);
        if (!tmplFile.open(QIODevice::ReadOnly)) {
            if (false) qDebug() << "Could not open template file:" << templateJsonPath;
            continue;
        }

        QByteArray data = tmplFile.readAll();
        if (false) qDebug() << "[SolutionTemplateManager] Read" << data.size() << "bytes from template file";
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            if (false) qDebug() << "Invalid JSON in template file:" << templateJsonPath;
            continue;
        }

        SolutionTemplate tmpl = SolutionTemplate::fromJson(doc.object());
        if (false) qDebug() << "[SolutionTemplateManager] Loaded template:" << tmpl.name() << "with" << tmpl.sequences().size() << "sequences";
        
        // Проверяем целостность шаблона и выводим предупреждение о несуществующих последовательностях
        auto sequences = tmpl.sequences(); // QVector<QString> 
        for (const QString& seqPath : sequences) {
            QString fullPath = QDir(templateDirPath).filePath(seqPath);
            if (!QFile::exists(fullPath)) {
                qWarning() << "Template" << info.fileName() << "references non-existent sequence:" << seqPath;
            }
        }
        
        m_templates.insert(templateDirPath, tmpl);
        m_templateDirs << templateDirPath;
        if (false) qDebug() << "[SolutionTemplateManager] Added template to cache, now have" << m_templates.size() << "templates";
    }
    if (false) qDebug() << "[SolutionTemplateManager] Refresh completed with" << m_templates.size() << "templates";
}

const QMap<QString, SolutionTemplate> &SolutionTemplateManager::templates()
{
    refresh();
    return m_templates;
}

/* Сохранить/перезаписать шаблон */
bool SolutionTemplateManager::save(const SolutionTemplate &tmpl, const QString &templateDirPath)
{
    QDir dir;
    if (!dir.mkpath(templateDirPath)) {
        if (false) qDebug() << "Could not create template directory:" << templateDirPath;
        return false;
    }

    // Сохраняем JSON-файл шаблона
    QJsonObject obj = tmpl.toJson();
    QJsonDocument doc(obj);
    QString templateJsonPath = templateDirPath + "/template.templ.json";
    QFile tmplFile(templateJsonPath);
    if (!tmplFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    tmplFile.write(doc.toJson(QJsonDocument::Indented));
    tmplFile.close();

    // Обновляем кеш
    m_templates.insert(templateDirPath, tmpl);
    if (!m_templateDirs.contains(templateDirPath))
        m_templateDirs << templateDirPath;
    
    return true;
}

/* Удалить шаблон */
bool SolutionTemplateManager::remove(const QString &templateDirPath)
{
    QDir dir(templateDirPath);
    if (!dir.exists())
        return false;

    // Удаляем всю директорию шаблона
    if (!dir.removeRecursively())
        return false;
        
    m_templates.remove(templateDirPath);
    m_templateDirs.removeAll(templateDirPath);
    return true;
}

/* Получить список доступных последовательностей для конкретного шаблона */
QStringList SolutionTemplateManager::getAvailableSequences(const QString &templateDirPath) const
{
    QStringList sequences;
    QDir dir(templateDirPath);
    if (!dir.exists())
        return sequences;

    // Ищем все файлы *.seq.json в директории шаблона
    QStringList filters = {"*.seq.json"};
    QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks);
    
    for (const QFileInfo& fileInfo : fileInfoList) {
        sequences << fileInfo.absoluteFilePath();
    }
    
    return sequences;
}

/* Получить информацию о главной последовательности внутри шаблона */
QString SolutionTemplateManager::getMainSequencePath(const QString &templateDirPath) const
{
    if (m_templates.contains(templateDirPath)) {
        SolutionTemplate tmpl = m_templates.value(templateDirPath);
        QString mainSeqPath = tmpl.mainSequencePath();
        
        // Если указан относительный путь к главной последовательности, делаем его абсолютным
        if (!mainSeqPath.isEmpty()) {
            // Check if the main sequence path exists as an absolute path
            if (QFileInfo::exists(mainSeqPath)) {
                return mainSeqPath;
            } else {
                // If it doesn't exist, try resolving it relative to the template directory
                QString resolvedPath = QDir(templateDirPath).absoluteFilePath(mainSeqPath);
                if (QFile::exists(resolvedPath)) {
                    return resolvedPath;
                } else {
                    // If still not found, return empty string to indicate it doesn't exist
                    return QString();
                }
            }
        }
        return mainSeqPath;
    }
    
    return QString();
}

/* Экспорт шаблона в ZIP-архив */
bool SolutionTemplateManager::exportTemplate(const QString& templateDirPath, const QString& exportPath) const
{
    QDir templateDir(templateDirPath);
    if (!templateDir.exists()) {
        if (false) qDebug() << "Template directory does not exist:" << templateDirPath;
        return false;
    }

    QFileInfo exportInfo(exportPath);
    QString targetDir = exportInfo.absolutePath() + "/" + exportInfo.baseName();

    if (QDir(targetDir).exists()) {
        QDir(targetDir).removeRecursively();
    }

    return copyRecursively(templateDirPath, targetDir);
}

/* Импорт шаблона из ZIP-архива */
bool SolutionTemplateManager::importTemplate(const QString& importPath, const QString& targetDir) const
{
    QFileInfo importInfo(importPath);
    if (!importInfo.exists())
        return false;

    QString sourceDir = importPath;
    if (importInfo.isFile()) {
        // ZIP extraction is not implemented in this build.
        return false;
    }

    if (QDir(targetDir).exists()) {
        QDir(targetDir).removeRecursively();
    }

    return copyRecursively(sourceDir, targetDir);
}

/* Экспорт последовательности как шаблона */
bool SolutionTemplateManager::exportSequenceAsTemplate(const QString& sequencePath, const QString& templateDirPath, 
                                                      const QString& templateName, const QString& description) const
{
    // Load the sequence from the file
    QFile seqFile(sequencePath);
    if (!seqFile.open(QIODevice::ReadOnly)) {
        if (false) qDebug() << "Could not open sequence file:" << sequencePath;
        return false;
    }
    
    QByteArray data = seqFile.readAll();
    QJsonDocument seqDoc = QJsonDocument::fromJson(data);
    if (!seqDoc.isObject()) {
        if (false) qDebug() << "Invalid JSON in sequence file:" << sequencePath;
        return false;
    }
    
    // Create template directory
    QDir dir;
    if (!dir.mkpath(templateDirPath)) {
        if (false) qDebug() << "Could not create template directory:" << templateDirPath;
        return false;
    }
    
    // Copy the sequence file to the template directory
    QFileInfo seqInfo(sequencePath);
    QString targetSeqPath = templateDirPath + "/" + seqInfo.fileName();
    if (!QFile::copy(sequencePath, targetSeqPath)) {
        if (false) qDebug() << "Could not copy sequence file to template directory:" << targetSeqPath;
        return false;
    }
    
    // Create template object
    SolutionTemplate tmpl(templateName);
    tmpl.setDescription(description.isEmpty() ? QString("Template created from sequence: %1").arg(seqInfo.fileName()) : description);
    tmpl.addSequence(seqInfo.fileName()); // Add relative path to sequence file
    tmpl.setMainSequencePath(seqInfo.fileName()); // Set the copied sequence as main sequence
    
    // Save template JSON
    QJsonObject tmplObj = tmpl.toJson();
    QJsonDocument tmplDoc(tmplObj);
    QFile tmplFile(templateDirPath + "/template.templ.json");
    if (!tmplFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (false) qDebug() << "Could not create template.templ.json file in:" << templateDirPath;
        return false;
    }
    tmplFile.write(tmplDoc.toJson(QJsonDocument::Indented));
    tmplFile.close();
    
    return true;
}

/* Создать новую последовательность в папке шаблона */
bool SolutionTemplateManager::createNewSequenceInTemplate(const QString& templateDirPath, 
                                                          const QString& sequenceName, 
                                                          const QString& description) const
{
    // Create default sequence structure
    QJsonObject sequenceObj;
    sequenceObj["name"] = sequenceName;
    sequenceObj["description"] = description.isEmpty() ? QString("New sequence: %1").arg(sequenceName) : description;
    
    QJsonArray stepsArray;  // Empty array for steps initially
    sequenceObj["steps"] = stepsArray;
    
    QJsonDocument sequenceDoc(sequenceObj);
    
    // Create file path
    QString sequenceFileName = sequenceName + ".seq.json";
    QString sequenceFilePath = QDir(templateDirPath).filePath(sequenceFileName);
    
    // Create the file and write the default sequence
    QFile sequenceFile(sequenceFilePath);
    if (!sequenceFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (false) qDebug() << "Could not create sequence file in template directory:" << sequenceFilePath;
        return false;
    }
    
    sequenceFile.write(sequenceDoc.toJson(QJsonDocument::Indented));
    sequenceFile.close();
    
    // Now update the template metadata to include this new sequence
    // First, load the existing template
    QString templateJsonPath = QDir(templateDirPath).filePath("template.templ.json");
    QFile tmplFile(templateJsonPath);
    if (!tmplFile.open(QIODevice::ReadOnly)) {
        if (false) qDebug() << "Could not open template file for updating:" << templateJsonPath;
        return true; // Return true since the sequence was created, just couldn't update template
    }
    
    QByteArray data = tmplFile.readAll();
    QJsonDocument tmplDoc = QJsonDocument::fromJson(data);
    if (!tmplDoc.isObject()) {
        if (false) qDebug() << "Invalid JSON in template file:" << templateJsonPath;
        return true; // Return true since the sequence was created, just couldn't update template
    }
    
    SolutionTemplate tmpl = SolutionTemplate::fromJson(tmplDoc.object());
    
    // Add our new sequence to the template
    tmpl.addSequence(sequenceFileName); // Add relative path to sequence file
    
    // Save the updated template
    QJsonObject updatedTmplObj = tmpl.toJson();
    QJsonDocument updatedTmplDoc(updatedTmplObj);
    
    QFile updatedTmplFile(templateJsonPath);
    if (!updatedTmplFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (false) qDebug() << "Could not update template file:" << templateJsonPath;
        return true; // Return true since the sequence was created, just couldn't update template
    }
    
    updatedTmplFile.write(updatedTmplDoc.toJson(QJsonDocument::Indented));
    updatedTmplFile.close();
    
    return true;
}

/* Синхронизировать файл шаблона с содержимым папки шаблона (добавить отсутствующие файлы последовательностей) */
bool SolutionTemplateManager::syncTemplateFileWithFolder(const QString& templateDirPath) const
{
    if (false) qDebug() << "[syncTemplateFileWithFolder] Starting sync for directory:" << templateDirPath;
    QDir dir(templateDirPath);
    if (!dir.exists()) {
        if (false) qDebug() << "[syncTemplateFileWithFolder] Template directory does not exist:" << templateDirPath;
        return false;
    }
    
    QString templateJsonPath = QDir(templateDirPath).filePath("template.templ.json");
    if (false) qDebug() << "[syncTemplateFileWithFolder] Template JSON path:" << templateJsonPath;
    
    // Check if template file exists, if not create a default one
    if (!QFile::exists(templateJsonPath)) {
        if (false) qDebug() << "[syncTemplateFileWithFolder] Template file does not exist, creating default";
        // Create a default template based on the sequence files in the directory
        SolutionTemplate tmpl(QFileInfo(templateDirPath).fileName()); // Use directory name as template name
        tmpl.setDescription("Default template created from sequence files in directory");
        
        // Find all sequence files in the directory
        QStringList seqFilters = {"*.seq.json"};
        QFileInfoList fileInfoList = dir.entryInfoList(seqFilters, QDir::Files | QDir::NoSymLinks);
        if (false) qDebug() << "[syncTemplateFileWithFolder] Found" << fileInfoList.size() << "sequence files";
        
        // Add all sequence files to the template
        for (const QFileInfo& fileInfo : fileInfoList) {
            if (false) qDebug() << "[syncTemplateFileWithFolder] Adding sequence file:" << fileInfo.fileName();
            tmpl.addSequence(fileInfo.fileName()); // Add relative path to sequence file
            
            // If this is the first sequence, set it as the main sequence
            if (tmpl.mainSequencePath().isEmpty()) {
                tmpl.setMainSequencePath(fileInfo.fileName());
                if (false) qDebug() << "[syncTemplateFileWithFolder] Set main sequence to:" << fileInfo.fileName();
            }
        }
        
        // If no sequence files found, create a minimal default
        if (tmpl.sequences().empty()) {
            if (false) qDebug() << "[syncTemplateFileWithFolder] No sequence files found, adding default";
            // Create a default sequence to ensure template is valid
            tmpl.addSequence("default.seq.json");
        }
        
        // Save the newly created template
        QJsonObject tmplObj = tmpl.toJson();
        QJsonDocument tmplDoc(tmplObj);
        QFile tmplFile(templateJsonPath);
        if (!tmplFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (false) qDebug() << "[syncTemplateFileWithFolder] Could not create template file:" << templateJsonPath;
            return false;
        }
        tmplFile.write(tmplDoc.toJson(QJsonDocument::Indented));
        tmplFile.close();
        
        if (false) qDebug() << "[syncTemplateFileWithFolder] Created default template file:" << templateJsonPath;
    } else {
        if (false) qDebug() << "[syncTemplateFileWithFolder] Template file exists, proceeding with sync";
    }
    
    // Now proceed with loading and synchronizing
    QFile tmplFile(templateJsonPath);
    if (!tmplFile.open(QIODevice::ReadOnly)) {
        if (false) qDebug() << "[syncTemplateFileWithFolder] Could not open template file for synchronization:" << templateJsonPath;
        return false;
    }
    
    QByteArray data = tmplFile.readAll();
    if (false) qDebug() << "[syncTemplateFileWithFolder] Read" << data.size() << "bytes from template file";
    QJsonDocument tmplDoc = QJsonDocument::fromJson(data);
    if (!tmplDoc.isObject()) {
        if (false) qDebug() << "[syncTemplateFileWithFolder] Invalid JSON in template file:" << templateJsonPath;
        return false;
    }
    
    SolutionTemplate tmpl = SolutionTemplate::fromJson(tmplDoc.object());
    if (false) qDebug() << "[syncTemplateFileWithFolder] Loaded template:" << tmpl.name() << "with" << tmpl.sequences().size() << "sequences";
    
    // Получаем список всех файлов последовательностей в папке шаблона
    QStringList seqFilters = {"*.seq.json"};
    QFileInfoList fileInfoList = dir.entryInfoList(seqFilters, QDir::Files | QDir::NoSymLinks);
    
    // Создаем список имен файлов последовательностей в папке
    QStringList folderSeqFiles;
    for (const QFileInfo& fileInfo : fileInfoList) {
        folderSeqFiles << fileInfo.fileName();
    }
    if (false) qDebug() << "[syncTemplateFileWithFolder] Found" << folderSeqFiles.size() << "sequence files in folder";
    
    // Проверяем, какие файлы уже есть в шаблоне
    auto existingSequences = tmpl.sequences();  // QVector<QString>
    if (false) qDebug() << "[syncTemplateFileWithFolder] Template currently has" << existingSequences.size() << "sequences";
    
    // Добавляем новые файлы, которых нет в шаблоне
    for (const QString& fileName : folderSeqFiles) {
        bool found = false;
        for (const QString& existingSeq : existingSequences) {
            if (existingSeq == fileName) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (false) qDebug() << "[syncTemplateFileWithFolder] Adding new sequence to template:" << fileName;
            // Добавляем файл к шаблону
            tmpl.addSequence(fileName);
        }
    }
    
    // Удаляем из шаблона файлы, которые больше не существуют в папке
    QVector<QString> seqsToRemove;
    for (const QString& seqPath : existingSequences) {
        if (!folderSeqFiles.contains(seqPath)) {
            if (false) qDebug() << "[syncTemplateFileWithFolder] Marking sequence for removal:" << seqPath;
            seqsToRemove.append(seqPath);
        }
    }
    for (const QString& seqToRemove : seqsToRemove) {
        if (false) qDebug() << "[syncTemplateFileWithFolder] Removing sequence from template:" << seqToRemove;
        tmpl.removeSequenceByPath(seqToRemove);
    }
    
    // Сохраняем обновленный шаблон
    QJsonObject updatedTmplObj = tmpl.toJson();
    QJsonDocument updatedTmplDoc(updatedTmplObj);
    
    QFile updatedTmplFile(templateJsonPath);
    if (!updatedTmplFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (false) qDebug() << "[syncTemplateFileWithFolder] Could not update template file:" << templateJsonPath;
        return false;
    }
    
    updatedTmplFile.write(updatedTmplDoc.toJson(QJsonDocument::Indented));
    updatedTmplFile.close();
    
    if (false) qDebug() << "[syncTemplateFileWithFolder] Successfully synchronized template";
    return true;
}

/* Проверить целостность шаблона - наличие всех файлов последовательностей */
QStringList SolutionTemplateManager::checkTemplateIntegrity(const QString& templateDirPath) const
{
    QStringList missingFiles;

    QDir dir(templateDirPath);
    if (!dir.exists()) {
        return missingFiles;
    }

    // Загружаем шаблон
    QString templateJsonPath = QDir(templateDirPath).filePath("template.templ.json");

    QFile tmplFile(templateJsonPath);
    if (!tmplFile.open(QIODevice::ReadOnly)) {
        return missingFiles;
    }

    QByteArray data = tmplFile.readAll();
    QJsonDocument tmplDoc = QJsonDocument::fromJson(data);
    if (!tmplDoc.isObject()) {
        return missingFiles;
    }

    SolutionTemplate tmpl = SolutionTemplate::fromJson(tmplDoc.object());

    // Проверяем, существуют ли все файлы последовательностей
    auto sequences = tmpl.sequences(); // QVector<QString>
    for (const QString& seqPath : sequences) {
        QString fullPath = QDir(templateDirPath).filePath(seqPath);
        if (!QFile::exists(fullPath)) {
            missingFiles << seqPath;
        }
    }

    return missingFiles;
}

/* Сохранить позиции последовательностей в шаблон */
void SolutionTemplateManager::savePositionsToTemplate(const QString& templateDirPath, const QMap<QString, QPointF>& positions)
{
    if (false) qDebug() << "[SolutionTemplateManager] Saving positions to template:" << templateDirPath;
    if (false) qDebug() << "[SolutionTemplateManager] Number of positions to save:" << positions.size();
    
    QString templateJsonPath = QDir(templateDirPath).filePath("template.templ.json");
    
    // Load the existing template
    QFile tmplFile(templateJsonPath);
    if (!tmplFile.open(QIODevice::ReadOnly)) {
        if (false) qDebug() << "[SolutionTemplateManager] Could not open template file for position update:" << templateJsonPath;
        return;
    }
    
    QByteArray data = tmplFile.readAll();
    QJsonDocument tmplDoc = QJsonDocument::fromJson(data);
    if (!tmplDoc.isObject()) {
        if (false) qDebug() << "[SolutionTemplateManager] Invalid JSON in template file:" << templateJsonPath;
        tmplFile.close();
        return;
    }
    
    tmplFile.close();
    
    // Deserialize the template
    SolutionTemplate tmpl = SolutionTemplate::fromJson(tmplDoc.object());
    
    // Update positions in the template
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        tmpl.setPositionForSequence(it.key(), it.value());
        if (false) qDebug() << "[SolutionTemplateManager] Set position for sequence:" << it.key() << "at" << it.value();
    }
    
    // Save the updated template back to file
    QJsonObject updatedTmplObj = tmpl.toJson();
    QJsonDocument updatedTmplDoc(updatedTmplObj);
    
    QFile updatedTmplFile(templateJsonPath);
    if (!updatedTmplFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (false) qDebug() << "[SolutionTemplateManager] Could not update template file for positions:" << templateJsonPath;
        return;
    }
    
    updatedTmplFile.write(updatedTmplDoc.toJson(QJsonDocument::Indented));
    updatedTmplFile.close();
    
    // Also update the cached template in memory
    m_templates.insert(templateDirPath, tmpl);
    
    if (false) qDebug() << "[SolutionTemplateManager] Successfully saved positions to template file:" << templateJsonPath;
}

/* Загрузить позиции последовательностей из шаблона */
QMap<QString, QPointF> SolutionTemplateManager::loadPositionsFromTemplate(const QString& templateDirPath) const
{
    if (false) qDebug() << "[SolutionTemplateManager] Loading positions from template:" << templateDirPath;
    
    QString templateJsonPath = QDir(templateDirPath).filePath("template.templ.json");
    
    // Load the template file
    QFile tmplFile(templateJsonPath);
    if (!tmplFile.open(QIODevice::ReadOnly)) {
        if (false) qDebug() << "[SolutionTemplateManager] Could not open template file for position loading:" << templateJsonPath;
        return QMap<QString, QPointF>(); // Return empty map
    }
    
    QByteArray data = tmplFile.readAll();
    QJsonDocument tmplDoc = QJsonDocument::fromJson(data);
    if (!tmplDoc.isObject()) {
        if (false) qDebug() << "[SolutionTemplateManager] Invalid JSON in template file:" << templateJsonPath;
        tmplFile.close();
        return QMap<QString, QPointF>(); // Return empty map
    }
    
    tmplFile.close();
    
    // Deserialize the template
    SolutionTemplate tmpl = SolutionTemplate::fromJson(tmplDoc.object());
    
    // Get positions from the template
    QMap<QString, QPointF> positions = tmpl.positions();
    
    if (false) qDebug() << "[SolutionTemplateManager] Loaded" << positions.size() << "positions from template";
    
    return positions;
}

