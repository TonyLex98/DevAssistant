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

/* Описание отдельного файла проекта (для сохранения в .dapro) */
struct FileDescription
{
    QString filePath;          // Абсолютный путь
    QDateTime lastModTime;     // Время последнего изменения
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


    // Конструктор‑по‑умолчанию
    DevAssManager() {}

    /* Операции с DevAss */
    bool createFolder();
    bool createDaproFile();
    bool generateDescrFiles();

    /* Конфигурация */
    void setBasePrompt(const QString &path, const QString &label);
    void addFileDescription(const QString &filePath);
    void markFileChanged(const QString &filePath);
    void setHistoryFile(const QString &historyPath);

    /* Сохраняем/загружаем .dapro */
    bool save();
    bool load();

    /* Вспомогательные геттеры */
    QDir devAssDir() const { return m_devAssDir; }
    QDir projectRoot() const { return m_projectRoot; }
    QList<FileDescription> fileDescriptions() const { return m_fileDescriptions; }

    /* Новые методы для работы с описанием файлов */
    FileDescription* findFileDescription(const QString& path);
    void updateFileDescription(const QString& path,
                               const QDateTime& modTime,
                               bool upToDate);

    void setProjectRoot(const QString &projectRoot);


private:
    QDir m_projectRoot;   // Корень выбранного проекта
    QDir m_devAssDir;     // Папка DevAss
    QFile m_daproFile;    // *.dapro

    QString m_basePromptPath;
    QString m_basePromptLabel;
    QString m_historyFilePath;

    QList<FileDescription> m_fileDescriptions;
};

#endif // DEVASSMANAGER_H
