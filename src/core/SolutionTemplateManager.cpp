// SolutionTemplateManager.cpp
#include "SolutionTemplateManager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QFileInfo>

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
    m_templates.clear();
    m_templateDirs.clear();

    if (m_root.isEmpty())
        return;

    QDir dir(m_root);
    if (!dir.exists())
        return;

    // Получаем все поддиректории (это будут папки шаблонов)
    QFileInfoList entryList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entryList) {
        QString templateDirPath = info.absoluteFilePath();
        QString templateJsonPath = templateDirPath + "/template.json";
        
        // Загружаем JSON-файл шаблона
        QFile tmplFile(templateJsonPath);
        if (!tmplFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Could not open template file:" << templateJsonPath;
            continue;
        }

        QByteArray data = tmplFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qDebug() << "Invalid JSON in template file:" << templateJsonPath;
            continue;
        }

        SolutionTemplate tmpl = SolutionTemplate::fromJson(doc.object());
        m_templates.insert(templateDirPath, tmpl);
        m_templateDirs << templateDirPath;
    }
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
        qDebug() << "Could not create template directory:" << templateDirPath;
        return false;
    }

    // Сохраняем JSON-файл шаблона
    QJsonObject obj = tmpl.toJson();
    QJsonDocument doc(obj);
    QString templateJsonPath = templateDirPath + "/template.json";
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
        if (!mainSeqPath.isEmpty() && !QFileInfo::exists(mainSeqPath)) {
            // Если путь не абсолютный, делаем его относительно папки шаблона
            if (!mainSeqPath.startsWith("/")) {
                return QDir(templateDirPath).absoluteFilePath(mainSeqPath);
            }
        }
        return mainSeqPath;
    }
    
    return QString();
}

/* Экспорт шаблона в ZIP-архив */
bool SolutionTemplateManager::exportTemplate(const QString& templateDirPath, const QString& exportPath) const
{
    // We'll use Qt's file system functions to create a ZIP archive
    // For now, this is a placeholder implementation that copies files to a new directory structure
    // In a real implementation, we would create an actual ZIP file
    
    // Check if template directory exists
    QDir templateDir(templateDirPath);
    if (!templateDir.exists()) {
        qDebug() << "Template directory does not exist:" << templateDirPath;
        return false;
    }
    
    // Copy the entire template directory to the export location
    QFileInfo exportInfo(exportPath);
    QString targetDir = exportInfo.absolutePath() + "/" + exportInfo.baseName();
    
    if (QDir(targetDir).exists()) {
        // Remove existing target directory
        QDir(targetDir).removeRecursively();
    }
    
    // Copy the template directory recursively
    return templateDir.mkpath(targetDir) && 
           QFile::copy(templateDirPath + "/template.json", targetDir + "/template.json");
}

/* Импорт шаблона из ZIP-архива */
bool SolutionTemplateManager::importTemplate(const QString& importPath, const QString& targetDir) const
{
    Q_UNUSED(importPath);
    Q_UNUSED(targetDir);
    
    // Placeholder implementation - in a real implementation, this would extract a ZIP file
    // and set up the template directory structure
    return false;
}

/* Экспорт последовательности как шаблона */
bool SolutionTemplateManager::exportSequenceAsTemplate(const QString& sequencePath, const QString& templateDirPath, 
                                                      const QString& templateName, const QString& description) const
{
    // Load the sequence from the file
    QFile seqFile(sequencePath);
    if (!seqFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open sequence file:" << sequencePath;
        return false;
    }
    
    QByteArray data = seqFile.readAll();
    QJsonDocument seqDoc = QJsonDocument::fromJson(data);
    if (!seqDoc.isObject()) {
        qDebug() << "Invalid JSON in sequence file:" << sequencePath;
        return false;
    }
    
    // Create template directory
    QDir dir;
    if (!dir.mkpath(templateDirPath)) {
        qDebug() << "Could not create template directory:" << templateDirPath;
        return false;
    }
    
    // Copy the sequence file to the template directory
    QFileInfo seqInfo(sequencePath);
    QString targetSeqPath = templateDirPath + "/" + seqInfo.fileName();
    if (!QFile::copy(sequencePath, targetSeqPath)) {
        qDebug() << "Could not copy sequence file to template directory:" << targetSeqPath;
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
    QFile tmplFile(templateDirPath + "/template.json");
    if (!tmplFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not create template.json file in:" << templateDirPath;
        return false;
    }
    tmplFile.write(tmplDoc.toJson(QJsonDocument::Indented));
    tmplFile.close();
    
    return true;
}