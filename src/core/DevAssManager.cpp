#include "DevAssManager.h"

DevAssManager::DevAssManager(const QString &projectRoot,
                             QObject *parent)
    : QObject(parent),
      m_projectRoot(projectRoot),
      m_devAssDir(projectRoot + "/DevelopersAssistant"),
      m_daproFile(m_devAssDir.absoluteFilePath("project.dapro"))
{
}

bool DevAssManager::applyChanges(QList<Change> changes, QString* errorOut)
{
    if (changes.isEmpty())
        return true;

    // Normalize paths to absolute project paths when possible.
    for (Change& c : changes) {
        if (!QFileInfo(c.targetPath).isAbsolute())
            c.targetPath = m_projectRoot.filePath(c.targetPath);
        if (!c.aux.isEmpty() && !QFileInfo(c.aux).isAbsolute())
            c.aux = m_projectRoot.filePath(c.aux);
    }

    QList<Change> copy = changes;
    if (!m_changeApplier.apply(copy, errorOut))
        return false;

    // Update file descriptions for touched files.
    for (const Change& c : copy) {
        if (c.type == Change::CreateFile || c.type == Change::ReplaceInFile) {
            updateFileDescription(c.targetPath, QDateTime::currentDateTime(), true);
        } else if (c.type == Change::DeleteFile) {
            findFileDescription(c.targetPath); // keep existing behavior (no remove)
        } else if (c.type == Change::RenameFile) {
            updateFileDescription(c.targetPath, QDateTime::currentDateTime(), true);
        }
    }

    save();
    return true;
}

bool DevAssManager::undoLastChange(QString* errorOut)
{
    const bool ok = m_changeApplier.undo(errorOut);
    if (ok)
        save();
    return ok;
}

void DevAssManager::setProjectRoot(const QString &projectRoot)
{
    m_projectRoot = QDir(projectRoot);
    m_devAssDir = QDir(projectRoot + "/DevelopersAssistant");
    m_daproFile.setFileName(m_devAssDir.absoluteFilePath("project.dapro"));
    //m_daproFile.open()

}

/* --------------------------------------------------------------- */
/* 1. Создание каталога DevAss                                    */
/* --------------------------------------------------------------- */
bool DevAssManager::createFolder()
{
    if (!m_devAssDir.exists())
    {
        if (!m_devAssDir.mkpath("."))
        {
            qWarning() << "Не удалось создать папку DevelopersAssistant:" << m_devAssDir.absolutePath();
            return false;
        }
    }
    return true;
}

/* --------------------------------------------------------------- */
/* 2. Создание/обновление .dapro                                   */
/* --------------------------------------------------------------- */
bool DevAssManager::createDaproFile()
{
    if (!m_devAssDir.exists())
    {
        if (!createFolder())
            return false;
    }

    if (!m_daproFile.exists())
    {
        if (!m_daproFile.open(QIODevice::WriteOnly))
        {
            qWarning() << "Не удалось создать .dapro файл:" << m_daproFile.fileName();
            return false;
        }
        m_daproFile.close();
    }
    return true;
}

/* --------------------------------------------------------------- */
/* 3. Генерация файлов‑дескрипторов *.dadescr                     */
/* --------------------------------------------------------------- */
bool DevAssManager::generateDescrFiles()
{
    if (!m_devAssDir.exists())
    {
        if (!createFolder())
            return false;
    }

    QDirIterator it(m_projectRoot.absolutePath(),
                    QDir::Files,   // только файлы
                    QDirIterator::Subdirectories);

    bool ok = true;

    while (it.hasNext())
    {
        it.next();

        QString srcFilePath = it.filePath();

        /* Не обрабатывать собственную папку DevelopersAssistant (иначе зациклимся) */
        if (srcFilePath.startsWith(m_devAssDir.absolutePath()))
            continue;

        /* Полный относительный путь от корня проекта */
        QString relPath = m_projectRoot.relativeFilePath(srcFilePath);

        /* Путь к файлу‑дескриптору в DevelopersAssistant */
        QString descrPath = m_devAssDir.absoluteFilePath(relPath + ".dadescr");
        QFileInfo descrInfo(descrPath);

        /* Создаём необходимые подкаталоги */
        QDir descrDir = descrInfo.dir();
        if (!descrDir.exists())
            descrDir.mkpath(".");

        /* Если дескриптор уже существует – пропускаем */
        if (descrInfo.exists())
            continue;

        /* Создаём пустой файл (можно позже заполнить текстом) */
        QFile descrFile(descrPath);
        if (!descrFile.open(QIODevice::WriteOnly))
        {
            qWarning() << "Не удалось создать дескриптор:" << descrPath;
            ok = false;
            continue;
        }
        descrFile.close();
    }

    return ok;
}

/* --------------------------------------------------------------- */
/* 4. Методы настройки конфигурации (.dapro)                       */
/* --------------------------------------------------------------- */
void DevAssManager::setBasePrompt(const QString &path, const QString &label)
{
    m_basePromptPath = path;
    m_basePromptLabel = label;
}

void DevAssManager::addFileDescription(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists())
        return;

    FileDescription fd;
    fd.filePath = filePath;
    fd.lastModTime = fi.lastModified();
    fd.isDescriptionUpToDate = true;

    for (auto &existing : m_fileDescriptions)
    {
        if (existing.filePath == filePath)
        {
            existing.lastModTime = fd.lastModTime;
            existing.isDescriptionUpToDate = true;
            return;
        }
    }

    m_fileDescriptions.append(fd);
}

void DevAssManager::markFileChanged(const QString &filePath)
{
    for (auto &fd : m_fileDescriptions)
    {
        if (fd.filePath == filePath)
        {
            fd.isDescriptionUpToDate = false;
            QFileInfo fi(filePath);
            fd.lastModTime = fi.lastModified();
            return;
        }
    }

    // Если файла ещё нет – добавляем
    addFileDescription(filePath);
    m_fileDescriptions.last().isDescriptionUpToDate = false;
}

void DevAssManager::setHistoryFile(const QString &historyPath)
{
    m_historyFilePath = historyPath;
}

/* --------------------------------------------------------------- */
/* 5. Сохранение/загрузка .dapro                                   */
/* --------------------------------------------------------------- */
bool DevAssManager::save()
{
    if (!m_devAssDir.exists())
    {
        if (!createFolder())
            return false;
    }

    QJsonObject rootObj;

    if (!m_basePromptPath.isEmpty())
    {
        rootObj["basePromptPath"] = m_basePromptPath;
        rootObj["basePromptLabel"] = m_basePromptLabel;
    }

    if (!m_historyFilePath.isEmpty())
        rootObj["historyPath"] = m_historyFilePath;

    QJsonArray filesArray;
    for (const auto &fd : m_fileDescriptions)
        filesArray.append(fd.toJson());
    rootObj["fileDescriptions"] = filesArray;

    QJsonDocument doc(rootObj);
    QByteArray jsonBytes = doc.toJson(QJsonDocument::Indented);

    if (!m_daproFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning() << "Не удалось открыть .dapro файл для записи:" << m_daproFile.fileName();
        return false;
    }

    m_daproFile.write(jsonBytes);
    m_daproFile.close();
    return true;
}

bool DevAssManager::load()
{
    if (!m_daproFile.exists())
        return false;

    if (!m_daproFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Не удалось открыть .dapro файл для чтения:" << m_daproFile.fileName();
        return false;
    }

    QByteArray jsonBytes = m_daproFile.readAll();
    m_daproFile.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        qWarning() << "Ошибка разбора JSON в .dapro:" << parseError.errorString();
        return false;
    }

    QJsonObject rootObj = doc.object();

    m_basePromptPath = rootObj.value("basePromptPath").toString();
    m_basePromptLabel = rootObj.value("basePromptLabel").toString();
    m_historyFilePath = rootObj.value("historyPath").toString();

    m_fileDescriptions.clear();
    QJsonArray filesArray = rootObj.value("fileDescriptions").toArray();
    for (const auto &elem : filesArray)
    {
        if (elem.isObject())
            m_fileDescriptions.append(FileDescription::fromJson(elem.toObject()));
    }

    return true;
}

FileDescription* DevAssManager::findFileDescription(const QString& path)
{
    for (FileDescription& fd : m_fileDescriptions) {
        if (fd.filePath == path)
            return &fd;
    }
    return nullptr;
}

void DevAssManager::updateFileDescription(const QString& path,
                                          const QDateTime& modTime,
                                          bool upToDate)
{
    FileDescription* fd = findFileDescription(path);
    if (fd) {
        fd->lastModTime = modTime;
        fd->isDescriptionUpToDate = upToDate;
    } else {
        FileDescription newFd;
        newFd.filePath = path;
        newFd.lastModTime = modTime;
        newFd.isDescriptionUpToDate = upToDate;
        m_fileDescriptions.append(newFd);
    }
}

