#ifndef DEVASSMANAGER_H
#define DEVASSMANAGER_H

#include <QObject>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDebug>
#include <QDirIterator>
#include <QDateTime>
#include "../core/ChangeApplier.h"

/* A single project file description (stored in .dapro) */
struct FileDescription
{
    QString filePath;          // absolute path
    QDateTime lastModTime;     // last modification time
    bool isDescriptionUpToDate = true;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["filePath"] = filePath;
        obj["lastModTime"] = lastModTime.toString(Qt::ISODate);
        obj["isDescriptionUpToDate"] = isDescriptionUpToDate;
        return obj;
    }

    static FileDescription fromJson(const QJsonObject &obj)
    {
        FileDescription fd;
        fd.filePath = obj["filePath"].toString();
        fd.lastModTime = QDateTime::fromString(obj["lastModTime"].toString(), Qt::ISODate);
        fd.isDescriptionUpToDate = obj["isDescriptionUpToDate"].toBool();
        return fd;
    }
};

class DevAssManager : public QObject
{
    Q_OBJECT
public:
    DevAssManager(const QString &projectRoot,
                           QObject *parent = nullptr);


    DevAssManager() {}

    /* DevelopersAssistant folder operations */
    bool createFolder();
    bool createDaproFile();
    bool generateDescrFiles();

    /* Configuration */
    void setBasePrompt(const QString &path, const QString &label);
    void addFileDescription(const QString &filePath);
    void markFileChanged(const QString &filePath);
    void setHistoryFile(const QString &historyPath);

    /* Save/load .dapro */
    bool save();
    bool load();

    /* Getters */
    QDir devAssDir() const { return m_devAssDir; }
    QDir projectRoot() const { return m_projectRoot; }
    QList<FileDescription> fileDescriptions() const { return m_fileDescriptions; }

    /* File descriptions */
    FileDescription* findFileDescription(const QString& path);
    void updateFileDescription(const QString& path,
                               const QDateTime& modTime,
                               bool upToDate);

    void setProjectRoot(const QString &projectRoot);

    // Apply and undo a batch of filesystem changes with a single-level undo stack.
    bool applyChanges(QList<Change> changes, QString* errorOut = nullptr);
    bool undoLastChange(QString* errorOut = nullptr);


private:
    QDir m_projectRoot;   // root of the selected project
    QDir m_devAssDir;     // DevelopersAssistant folder
    QFile m_daproFile;    // *.dapro

    QString m_basePromptPath;
    QString m_basePromptLabel;
    QString m_historyFilePath;

    QList<FileDescription> m_fileDescriptions;
    ChangeApplier m_changeApplier;
};

#endif // DEVASSMANAGER_H
