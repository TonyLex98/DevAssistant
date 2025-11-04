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

    /* Папка, где находятся шаблоны решений */
    void setRoot(const QString& rootPath);          // <project>/DevelopersAssistant/SolutionTemplates
    QString root() const { return m_root; }

    /* Сканирование всех папок с шаблонами */
    void refresh();

    /* Доступ к уже загруженным объектам */
    const QMap<QString, SolutionTemplate>& templates(); // key = full path to template directory

    /* Сохранить/удалить отдельный шаблон */
    bool save(const SolutionTemplate& tmpl, const QString& templateDirPath);
    bool remove(const QString& templateDirPath);

    /* Получить список папок (полные пути) – удобно для UI-выбора */
    QStringList templateDirs() const { return m_templateDirs; }

    /* Получить список доступных последовательностей для конкретного шаблона */
    QStringList getAvailableSequences(const QString& templateDirPath) const;

    /* Получить информацию о последовательности внутри шаблона */
    QString getMainSequencePath(const QString& templateDirPath) const;

    /* Экспорт шаблона в ZIP-архив */
    bool exportTemplate(const QString& templateDirPath, const QString& exportPath) const;
    
    /* Импорт шаблона из ZIP-архив */
    bool importTemplate(const QString& importPath, const QString& targetDir) const;
    
    /* Экспорт последовательности как шаблона */
    bool exportSequenceAsTemplate(const QString& sequencePath, const QString& templateDirPath, 
                                  const QString& templateName, const QString& description = QString()) const;
    
    /* Создать новую последовательность в папке шаблона */
    bool createNewSequenceInTemplate(const QString& templateDirPath, const QString& sequenceName, 
                                     const QString& description = QString()) const;
    
    /* Синхронизировать файл шаблона с содержимым папки шаблона (добавить отсутствующие файлы последовательностей) */
    bool syncTemplateFileWithFolder(const QString& templateDirPath) const;
    
    /* Проверить целостность шаблона - наличие всех файлов последовательностей */
    QStringList checkTemplateIntegrity(const QString& templateDirPath) const;

    /* Access to sequence manager for template editor */
    void setSequenceManager(PromptSequenceManager* seqMgr) { m_seqMgr = seqMgr; }
    PromptSequenceManager* sequenceManager() { return m_seqMgr; }

private:
    QString m_root;                                  // директория SolutionTemplates
    QMap<QString, SolutionTemplate> m_templates;     // key = абсолютный путь к папке шаблона
    QStringList m_templateDirs;                      // список папок шаблонов
    PromptSequenceManager* m_seqMgr = nullptr;       // pointer to sequence manager
};