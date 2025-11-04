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

    // РџРѕР»СѓС‡Р°РµРј РІСЃРµ РїРѕРґРґРёСЂРµРєС‚РѕСЂРёРё (СЌС‚Рѕ Р±СѓРґСѓС‚ РїР°РїРєРё С€Р°Р±Р»РѕРЅРѕРІ)
    QFileInfoList entryList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entryList) {
        QString templateDirPath = info.absoluteFilePath();
        
        // РЎРёРЅС…СЂРѕРЅРёР·РёСЂСѓРµРј СЃРѕРґРµСЂР¶РёРјРѕРµ РїР°РїРєРё С€Р°Р±Р»РѕРЅР° СЃ С„Р°Р№Р»РѕРј С€Р°Р±Р»РѕРЅР°
        syncTemplateFileWithFolder(templateDirPath);
        
        QString templateJsonPath = templateDirPath + "/template.templ.json";
        
        // Р—Р°РіСЂСѓР¶Р°РµРј JSON-С„Р°Р№Р» С€Р°Р±Р»РѕРЅР°
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
        
        // РџСЂРѕРІРµСЂСЏРµРј С†РµР»РѕСЃС‚РЅРѕСЃС‚СЊ С€Р°Р±Р»РѕРЅР° Рё РІС‹РІРѕРґРёРј РїСЂРµРґСѓРїСЂРµР¶РґРµРЅРёРµ Рѕ РЅРµСЃСѓС‰РµСЃС‚РІСѓСЋС‰РёС… РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚СЏС…
        auto sequences = tmpl.sequences(); // QVector<QString> 
        for (const QString& seqPath : sequences) {
            QString fullPath = QDir(templateDirPath).filePath(seqPath);
            if (!QFile::exists(fullPath)) {
                qWarning() << "Template" << info.fileName() << "references non-existent sequence:" << seqPath;
            }
        }
        
        m_templates.insert(templateDirPath, tmpl);
        m_templateDirs << templateDirPath;
    }
}

const QMap<QString, SolutionTemplate> &SolutionTemplateManager::templates()
{
    refresh();
    return m_templates;
}

/* РЎРѕС…СЂР°РЅРёС‚СЊ/РїРµСЂРµР·Р°РїРёСЃР°С‚СЊ С€Р°Р±Р»РѕРЅ */
bool SolutionTemplateManager::save(const SolutionTemplate &tmpl, const QString &templateDirPath)
{
    QDir dir;
    if (!dir.mkpath(templateDirPath)) {
        qDebug() << "Could not create template directory:" << templateDirPath;
        return false;
    }

    // РЎРѕС…СЂР°РЅСЏРµРј JSON-С„Р°Р№Р» С€Р°Р±Р»РѕРЅР°
    QJsonObject obj = tmpl.toJson();
    QJsonDocument doc(obj);
    QString templateJsonPath = templateDirPath + "/template.templ.json";
    QFile tmplFile(templateJsonPath);
    if (!tmplFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    tmplFile.write(doc.toJson(QJsonDocument::Indented));
    tmplFile.close();

    // РћР±РЅРѕРІР»СЏРµРј РєРµС€
    m_templates.insert(templateDirPath, tmpl);
    if (!m_templateDirs.contains(templateDirPath))
        m_templateDirs << templateDirPath;
    
    return true;
}

/* РЈРґР°Р»РёС‚СЊ С€Р°Р±Р»РѕРЅ */
bool SolutionTemplateManager::remove(const QString &templateDirPath)
{
    QDir dir(templateDirPath);
    if (!dir.exists())
        return false;

    // РЈРґР°Р»СЏРµРј РІСЃСЋ РґРёСЂРµРєС‚РѕСЂРёСЋ С€Р°Р±Р»РѕРЅР°
    if (!dir.removeRecursively())
        return false;
        
    m_templates.remove(templateDirPath);
    m_templateDirs.removeAll(templateDirPath);
    return true;
}

/* РџРѕР»СѓС‡РёС‚СЊ СЃРїРёСЃРѕРє РґРѕСЃС‚СѓРїРЅС‹С… РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚РµР№ РґР»СЏ РєРѕРЅРєСЂРµС‚РЅРѕРіРѕ С€Р°Р±Р»РѕРЅР° */
QStringList SolutionTemplateManager::getAvailableSequences(const QString &templateDirPath) const
{
    QStringList sequences;
    QDir dir(templateDirPath);
    if (!dir.exists())
        return sequences;

    // РС‰РµРј РІСЃРµ С„Р°Р№Р»С‹ *.seq.json РІ РґРёСЂРµРєС‚РѕСЂРёРё С€Р°Р±Р»РѕРЅР°
    QStringList filters = {"*.seq.json"};
    QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks);
    
    for (const QFileInfo& fileInfo : fileInfoList) {
        sequences << fileInfo.absoluteFilePath();
    }
    
    return sequences;
}

/* РџРѕР»СѓС‡РёС‚СЊ РёРЅС„РѕСЂРјР°С†РёСЋ Рѕ РіР»Р°РІРЅРѕР№ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚Рё РІРЅСѓС‚СЂРё С€Р°Р±Р»РѕРЅР° */
QString SolutionTemplateManager::getMainSequencePath(const QString &templateDirPath) const
{
    if (m_templates.contains(templateDirPath)) {
        SolutionTemplate tmpl = m_templates.value(templateDirPath);
        QString mainSeqPath = tmpl.mainSequencePath();
        
        // Р•СЃР»Рё СѓРєР°Р·Р°РЅ РѕС‚РЅРѕСЃРёС‚РµР»СЊРЅС‹Р№ РїСѓС‚СЊ Рє РіР»Р°РІРЅРѕР№ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚Рё, РґРµР»Р°РµРј РµРіРѕ Р°Р±СЃРѕР»СЋС‚РЅС‹Рј
        if (!mainSeqPath.isEmpty() && !QFileInfo::exists(mainSeqPath)) {
            // Р•СЃР»Рё РїСѓС‚СЊ РЅРµ Р°Р±СЃРѕР»СЋС‚РЅС‹Р№, РґРµР»Р°РµРј РµРіРѕ РѕС‚РЅРѕСЃРёС‚РµР»СЊРЅРѕ РїР°РїРєРё С€Р°Р±Р»РѕРЅР°
            if (!mainSeqPath.startsWith("/")) {
                return QDir(templateDirPath).absoluteFilePath(mainSeqPath);
            }
        }
        return mainSeqPath;
    }
    
    return QString();
}

/* Р­РєСЃРїРѕСЂС‚ С€Р°Р±Р»РѕРЅР° РІ ZIP-Р°СЂС…РёРІ */
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
           QFile::copy(templateDirPath + "/template.templ.json", targetDir + "/template.templ.json");
}

/* РРјРїРѕСЂС‚ С€Р°Р±Р»РѕРЅР° РёР· ZIP-Р°СЂС…РёРІР° */
bool SolutionTemplateManager::importTemplate(const QString& importPath, const QString& targetDir) const
{
    Q_UNUSED(importPath);
    Q_UNUSED(targetDir);
    
    // Placeholder implementation - in a real implementation, this would extract a ZIP file
    // and set up the template directory structure
    return false;
}

/* Р­РєСЃРїРѕСЂС‚ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚Рё РєР°Рє С€Р°Р±Р»РѕРЅР° */
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
    QFile tmplFile(templateDirPath + "/template.templ.json");
    if (!tmplFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not create template.templ.json file in:" << templateDirPath;
        return false;
    }
    tmplFile.write(tmplDoc.toJson(QJsonDocument::Indented));
    tmplFile.close();
    
    return true;
}

/* РЎРѕР·РґР°С‚СЊ РЅРѕРІСѓСЋ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚СЊ РІ РїР°РїРєРµ С€Р°Р±Р»РѕРЅР° */
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
        qDebug() << "Could not create sequence file in template directory:" << sequenceFilePath;
        return false;
    }
    
    sequenceFile.write(sequenceDoc.toJson(QJsonDocument::Indented));
    sequenceFile.close();
    
    // Now update the template metadata to include this new sequence
    // First, load the existing template
    QString templateJsonPath = QDir(templateDirPath).filePath("template.templ.json");
    QFile tmplFile(templateJsonPath);
    if (!tmplFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open template file for updating:" << templateJsonPath;
        return true; // Return true since the sequence was created, just couldn't update template
    }
    
    QByteArray data = tmplFile.readAll();
    QJsonDocument tmplDoc = QJsonDocument::fromJson(data);
    if (!tmplDoc.isObject()) {
        qDebug() << "Invalid JSON in template file:" << templateJsonPath;
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
        qDebug() << "Could not update template file:" << templateJsonPath;
        return true; // Return true since the sequence was created, just couldn't update template
    }
    
    updatedTmplFile.write(updatedTmplDoc.toJson(QJsonDocument::Indented));
    updatedTmplFile.close();
    
    return true;
}

/* РЎРёРЅС…СЂРѕРЅРёР·РёСЂРѕРІР°С‚СЊ С„Р°Р№Р» С€Р°Р±Р»РѕРЅР° СЃ СЃРѕРґРµСЂР¶РёРјС‹Рј РїР°РїРєРё С€Р°Р±Р»РѕРЅР° (РґРѕР±Р°РІРёС‚СЊ РѕС‚СЃСѓС‚СЃС‚РІСѓСЋС‰РёРµ С„Р°Р№Р»С‹ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚РµР№) */
bool SolutionTemplateManager::syncTemplateFileWithFolder(const QString& templateDirPath) const
{
    QDir dir(templateDirPath);
    if (!dir.exists()) {
        qDebug() << "Template directory does not exist:" << templateDirPath;
        return false;
    }
    
    // Р—Р°РіСЂСѓР¶Р°РµРј СЃСѓС‰РµСЃС‚РІСѓСЋС‰РёР№ С€Р°Р±Р»РѕРЅ
    QString templateJsonPath = QDir(templateDirPath).filePath("template.templ.json");
    QFile tmplFile(templateJsonPath);
    if (!tmplFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open template file for synchronization:" << templateJsonPath;
        return false;
    }
    
    QByteArray data = tmplFile.readAll();
    QJsonDocument tmplDoc = QJsonDocument::fromJson(data);
    if (!tmplDoc.isObject()) {
        qDebug() << "Invalid JSON in template file:" << templateJsonPath;
        return false;
    }
    
    SolutionTemplate tmpl = SolutionTemplate::fromJson(tmplDoc.object());
    
    // РџРѕР»СѓС‡Р°РµРј СЃРїРёСЃРѕРє РІСЃРµС… С„Р°Р№Р»РѕРІ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚РµР№ РІ РїР°РїРєРµ С€Р°Р±Р»РѕРЅР°
    QStringList seqFilters = {"*.seq.json"};
    QFileInfoList fileInfoList = dir.entryInfoList(seqFilters, QDir::Files | QDir::NoSymLinks);
    
    // РЎРѕР·РґР°РµРј СЃРїРёСЃРѕРє РёРјРµРЅ С„Р°Р№Р»РѕРІ РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕСЃС‚РµР№ РІ РїР°РїРєРµ
    QStringList folderSeqFiles;
    for (const QFileInfo& fileInfo : fileInfoList) {
        folderSeqFiles << fileInfo.fileName();
    }
    
    // РџСЂРѕРІРµСЂСЏРµРј, РєР°РєРёРµ С„Р°Р№Р»С‹ СѓР¶Рµ РµСЃС‚СЊ РІ С€Р°Р±Р»РѕРЅРµ
    auto existingSequences = tmpl.sequences();  // QVector<QString>
    
    // Р”РѕР±Р°РІР»СЏРµРј РЅРѕРІС‹Рµ С„Р°Р№Р»С‹, РєРѕС‚РѕСЂС‹С… РЅРµС‚ РІ С€Р°Р±Р»РѕРЅРµ
    for (const QString& fileName : folderSeqFiles) {
        bool found = false;
        for (const QString& existingSeq : existingSequences) {
            if (existingSeq == fileName) {
                found = true;
                break;
            }
        }
        if (!found) {
            // Р”РѕР±Р°РІР»СЏРµРј С„Р°Р№Р» Рє С€Р°Р±Р»РѕРЅСѓ
            tmpl.addSequence(fileName);
        }
    }
    
    // РЈРґР°Р»СЏРµРј РёР· С€Р°Р±Р»РѕРЅР° С„Р°Р№Р»С‹, РєРѕС‚РѕСЂС‹Рµ Р±РѕР»СЊС€Рµ РЅРµ СЃСѓС‰РµСЃС‚РІСѓСЋС‚ РІ РїР°РїРєРµ
    QVector<QString> seqsToRemove;
    for (const QString& seqPath : existingSequences) {
        if (!folderSeqFiles.contains(seqPath)) {
            seqsToRemove.append(seqPath);
        }
    }
    for (const QString& seqToRemove : seqsToRemove) {
        tmpl.removeSequenceByPath(seqToRemove);
    }
    
    // РЎРѕС…СЂР°РЅСЏРµРј РѕР±РЅРѕРІР»РµРЅРЅС‹Р№ С€Р°Р±Р»РѕРЅ
    QJsonObject updatedTmplObj = tmpl.toJson();
    QJsonDocument updatedTmplDoc(updatedTmplObj);
    
    QFile updatedTmplFile(templateJsonPath);
    if (!updatedTmplFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not update template file:" << templateJsonPath;
        return false;
    }
    
    updatedTmplFile.write(updatedTmplDoc.toJson(QJsonDocument::Indented));
    updatedTmplFile.close();
    
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
