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

    DevAssManager(){

    };

    void setProjectRoot(const QString &projectRoot);

    /* 1. Создать каталог DevAss (если его ещё нет) */
    bool createFolder();

    /* 2. Создать/обновить .dapro (см. описание выше) */
    bool createDaproFile();

    /* 3. Создать файлы‑дескрипторы (*.dadescr) */
    bool generateDescrFiles();

    /* 4. Установить базовый промпт */
    void setBasePrompt(const QString &path, const QString &label);

    /* 5. Добавить/обновить описание файла (для .dapro) */
    void addFileDescription(const QString &filePath);
    void markFileChanged(const QString &filePath);
    void setHistoryFile(const QString &historyPath);


    /* 6. Сохранить/загрузить .dapro */
    bool save();
    bool load();

    /* Вспомогательные геттеры */
    QDir devAssDir() const { return m_devAssDir; }
    QList<FileDescription> fileDescriptions() const { return m_fileDescriptions; }

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
