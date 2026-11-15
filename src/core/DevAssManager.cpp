#include "DevAssManager.h"
#include "utils/Logging.h"
#include "utils/PathGuard.h"

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

    // Every change must stay inside the open project. Commands are checked before they run
    // (CommandPolicy); this is the last line of defence right before the disk is touched.
    const QString root = m_projectRoot.path();
    if (root.isEmpty() || root == QLatin1String(".")) {
        if (errorOut)
            *errorOut = tr("No project is open.");
        return false;
    }
    const QString absRoot = m_projectRoot.absolutePath();
    for (Change& c : changes) {
        if (!QFileInfo(c.targetPath).isAbsolute())
            c.targetPath = m_projectRoot.filePath(c.targetPath);
        if (!c.aux.isEmpty() && !QFileInfo(c.aux).isAbsolute())
            c.aux = m_projectRoot.filePath(c.aux);
        QString outside;
        if (!PathGuard::isInside(absRoot, c.targetPath))
            outside = c.targetPath;
        else if (!c.aux.isEmpty() && !PathGuard::isInside(absRoot, c.aux))
            outside = c.aux;
        if (!outside.isEmpty()) {
            if (errorOut)
                *errorOut = tr("%1 is outside the project folder.").arg(QDir::toNativeSeparators(outside));
            return false;
        }
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
/* 1. Create the DevAss folder                                    */
/* --------------------------------------------------------------- */
bool DevAssManager::createFolder()
{
    if (!m_devAssDir.exists())
    {
        if (!m_devAssDir.mkpath("."))
        {
            qCWarning(logCore) << "Failed to create the DevelopersAssistant folder:" << m_devAssDir.absolutePath();
            return false;
        }
    }
    return true;
}

/* --------------------------------------------------------------- */
/* 2. Create/update .dapro                                         */
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
            qCWarning(logCore) << "Failed to create the .dapro file:" << m_daproFile.fileName();
            return false;
        }
        m_daproFile.close();
    }
    return true;
}

/* --------------------------------------------------------------- */
/* 3. Generate the *.dadescr descriptor files                     */
/* --------------------------------------------------------------- */
bool DevAssManager::generateDescrFiles()
{
    if (!m_devAssDir.exists())
    {
        if (!createFolder())
            return false;
    }

    QDirIterator it(m_projectRoot.absolutePath(),
                    QDir::Files,
                    QDirIterator::Subdirectories);

    bool ok = true;

    while (it.hasNext())
    {
        it.next();

        QString srcFilePath = it.filePath();

        /* Skip our own DevelopersAssistant folder (otherwise we would loop) */
        if (srcFilePath.startsWith(m_devAssDir.absolutePath()))
            continue;

        QString relPath = m_projectRoot.relativeFilePath(srcFilePath);

        /* Descriptor file path inside DevelopersAssistant */
        QString descrPath = m_devAssDir.absoluteFilePath(relPath + ".dadescr");
        QFileInfo descrInfo(descrPath);

        QDir descrDir = descrInfo.dir();
        if (!descrDir.exists())
            descrDir.mkpath(".");

        if (descrInfo.exists())
            continue;

        /* Create an empty file (filled with text later) */
        QFile descrFile(descrPath);
        if (!descrFile.open(QIODevice::WriteOnly))
        {
            qCWarning(logCore) << "Failed to create the descriptor:" << descrPath;
            ok = false;
            continue;
        }
        descrFile.close();
    }

    return ok;
}

/* --------------------------------------------------------------- */
/* 4. Configuration setters (.dapro)                               */
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

    // Not tracked yet: add it. addFileDescription() skips files that do not exist, so only
    // mark the new entry, never whatever happens to be last in the list.
    const int before = m_fileDescriptions.size();
    addFileDescription(filePath);
    if (m_fileDescriptions.size() > before)
        m_fileDescriptions.last().isDescriptionUpToDate = false;
}

void DevAssManager::setHistoryFile(const QString &historyPath)
{
    m_historyFilePath = historyPath;
}

/* --------------------------------------------------------------- */
/* 5. Save/load .dapro                                             */
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
        qCWarning(logCore) << "Failed to open the .dapro file for writing:" << m_daproFile.fileName();
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
        qCWarning(logCore) << "Failed to open the .dapro file for reading:" << m_daproFile.fileName();
        return false;
    }

    QByteArray jsonBytes = m_daproFile.readAll();
    m_daproFile.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        qCWarning(logCore) << "Failed to parse the .dapro JSON:" << parseError.errorString();
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

