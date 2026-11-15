#pragma once
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include "core/project/DevAssManager.h"
#include "core/project/ProjectIndex.h"

// The project folder the assistant works on: its files, the changes made to them (with
// undo) and the file descriptions kept in <project>/DevelopersAssistant.
class Project : public QObject
{
    Q_OBJECT
public:
    explicit Project(QObject* parent = nullptr);

    QString root() const { return m_root; }
    bool isOpen() const { return !m_root.isEmpty(); }

    // Opens a folder: prepares the DevelopersAssistant folder and indexes the files.
    void open(const QString& folder);
    // Reads the folder and the descriptions again.
    void rescan();

    // Applies changes with undo and keeps the descriptions; every file command goes through it.
    DevAssManager& manager() { return m_manager; }
    const ProjectIndex& index() const { return m_index; }

    // Project files relative to the root, without the DevelopersAssistant folder.
    QStringList files() const;
    // Absolute paths of the files whose description is missing or older than the file.
    QSet<QString> filesNeedingDescription();

    // Texts for the prompts.
    QString fileListText() const;      // absolute paths, one per line
    QString structureText() const;     // the folder tree
    QString descriptionsText() const;  // every file description

signals:
    void opened(const QString& root);
    void scanned();

private:
    bool isInAssistantFolder(const QString& absolutePath) const;

    QString m_root;
    DevAssManager m_manager;
    ProjectIndex m_index;
};
