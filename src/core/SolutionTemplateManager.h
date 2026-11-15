#pragma once
#include <QObject>
#include <QMap>
#include <QString>
#include "SolutionTemplate.h"
#include <QDirIterator>
#include "../core/PromptSequenceManager.h"  // Include the sequence manager header

class SolutionTemplateManager : public QObject
{
    Q_OBJECT
public:
    explicit SolutionTemplateManager(QObject *parent = nullptr);

    /* Folder that holds the solution templates */
    void setRoot(const QString& rootPath);          // <project>/DevelopersAssistant/SolutionTemplates
    QString root() const { return m_root; }

    /* Rescan all template folders */
    void refresh();

    /* Already loaded templates */
    const QMap<QString, SolutionTemplate>& templates(); // key = full path to template directory

    /* Save/delete a single template */
    bool save(const SolutionTemplate& tmpl, const QString& templateDirPath);
    bool remove(const QString& templateDirPath);

    /* Template folders (full paths), handy for UI selection */
    QStringList templateDirs() const { return m_templateDirs; }

    /* Sequences available in the given template */
    QStringList getAvailableSequences(const QString& templateDirPath) const;

    /* Path of the template's main sequence */
    QString getMainSequencePath(const QString& templateDirPath) const;

    /* Export a template to a ZIP archive */
    bool exportTemplate(const QString& templateDirPath, const QString& exportPath) const;
    
    /* Import a template from a ZIP archive */
    bool importTemplate(const QString& importPath, const QString& targetDir) const;
    
    /* Export a sequence as a template */
    bool exportSequenceAsTemplate(const QString& sequencePath, const QString& templateDirPath, 
                                  const QString& templateName, const QString& description = QString()) const;
    
    /* Create a new sequence in the template folder */
    bool createNewSequenceInTemplate(const QString& templateDirPath, const QString& sequenceName, 
                                     const QString& description = QString()) const;
    
    /* Sync the template file with the template folder (add missing sequence files) */
    bool syncTemplateFileWithFolder(const QString& templateDirPath) const;
    
    /* Template integrity check: all sequence files must exist */
    QStringList checkTemplateIntegrity(const QString& templateDirPath) const;
    
    /* Sequence positions on the canvas */
    void savePositionsToTemplate(const QString& templateDirPath, const QMap<QString, QPointF>& positions);
    QMap<QString, QPointF> loadPositionsFromTemplate(const QString& templateDirPath) const;

    /* Access to sequence manager for template editor */
    void setSequenceManager(PromptSequenceManager* seqMgr) { m_seqMgr = seqMgr; }
    PromptSequenceManager* sequenceManager() { return m_seqMgr; }

private:
    QString m_root;                                  // SolutionTemplates directory
    QMap<QString, SolutionTemplate> m_templates;     // key = absolute path to the template folder
    QStringList m_templateDirs;                      // template folders
    PromptSequenceManager* m_seqMgr = nullptr;       // pointer to sequence manager
};