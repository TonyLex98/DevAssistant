// PromptSequenceManager.h
#pragma once
#include <QObject>
#include <QMap>
#include <QString>
#include "PromptSequence.h"
#include <QDirIterator>

class PromptSequenceManager : public QObject
{
    Q_OBJECT
public:
    explicit PromptSequenceManager(QObject *parent = nullptr);

    /* Folder that holds the sequences */
    void setRoot(const QString& rootPath);          // <project>/DevelopersAssistant/PromptSequences
    QString root() const { return m_root; }

    /* Scan all *.seq.json in the folder and its subfolders */
    void refresh();

    /* Already loaded sequences */
    const QMap<QString, PromptSequence>& sequences(); // key = full path

    /* Save/delete a single sequence */
    bool save(const PromptSequence& seq, const QString& filePath);
    bool remove(const QString& filePath);

    /* File list (full paths), handy for UI selection */
    QStringList fileList() const { return m_fileList; }

    /* Add/update a single sequence in the cache (used with templates) */
    void addSequenceToCache(const QString& filePath, const PromptSequence& sequence);
    
    /* Find a sequence by name (returns the file path and the sequence) */
    QPair<QString, PromptSequence> findSequenceByName(const QString& name) const;

private:
    QString m_root;                     // PromptSequences directory
    QMap<QString, PromptSequence> m_sequences; // key = absolute file path
    QStringList m_fileList;
    QMap<QString, PromptSequence> m_cachedExtra; // added via addSequenceToCache, kept on refresh()
};
