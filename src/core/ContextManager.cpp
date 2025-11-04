#include "ContextManager.h"
#include "../command/Command.h"
#include "../core/DevAssManager.h"
#include "../ui/MainWindow.h"
#include <QDateTime>
#include <QFileInfo>
#include <QDebug>

ContextManager::ContextManager(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_devAssManager(nullptr)
    , m_mainWindow(nullptr)
    , m_commandCounter(0)
{
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &ContextManager::onFileChanged);
}

ContextManager::~ContextManager()
{
    // Cleanup is handled by parent-child relationship
}

void ContextManager::addCommand(Command* cmd, const QString& result)
{
    if (!cmd) return;
    
    QString uniqueId = generateUniqueId(cmd->name());
    CommandContextInfo info;
    info.id = uniqueId;
    info.name = cmd->name();
    info.params = cmd->params();
    info.description = cmd->description();
    info.timestamp = QDateTime::currentDateTime();
    info.result = result;
    
    m_commandHistory.insert(uniqueId, info);
    
    // If command involves file operations, add those files to watcher
    QString filePath = info.params.value("path").toString();
    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        if (fileInfo.exists()) {
            m_fileWatcher->addPath(fileInfo.absoluteFilePath());
        }
    }
    
    emit commandAdded(uniqueId);
    emit contextChanged();
}

void ContextManager::invalidateCommand(const QString& commandId, const QString& reason)
{
    if (m_commandHistory.contains(commandId)) {
        CommandContextInfo& info = m_commandHistory[commandId];
        info.isStillValid = false;
        info.invalidationReason = reason;
        
        emit commandInvalidated(commandId, reason);
        emit contextChanged();
    }
}

void ContextManager::removeCommand(const QString& commandId)
{
    if (m_commandHistory.remove(commandId) > 0) {
        emit commandRemoved(commandId);
        emit contextChanged();
    }
}

QList<CommandContextInfo> ContextManager::getValidCommands() const
{
    QList<CommandContextInfo> validCommands;
    for (auto it = m_commandHistory.constBegin(); it != m_commandHistory.constEnd(); ++it) {
        if (it.value().isStillValid) {
            validCommands.append(it.value());
        }
    }
    return validCommands;
}

QList<CommandContextInfo> ContextManager::getAllCommands() const
{
    QList<CommandContextInfo> allCommands;
    for (auto it = m_commandHistory.constBegin(); it != m_commandHistory.constEnd(); ++it) {
        allCommands.append(it.value());
    }
    return allCommands;
}

bool ContextManager::isFileModifiedSinceCommand(const QString& filePath, const QDateTime& commandTime) const
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return true; // If file doesn't exist, it's considered modified
    }
    return fileInfo.lastModified() > commandTime;
}

QString ContextManager::generateContextText() const
{
    QStringList contextParts;
    
    // Add context from valid commands
    for (const auto& cmdInfo : getValidCommands()) {
        if (!cmdInfo.result.isEmpty()) {
            contextParts << cmdInfo.result;
        }
    }
    
    return contextParts.join("\n\n");
}

void ContextManager::clearAll()
{
    m_commandHistory.clear();
    m_fileWatcher->removePaths(m_fileWatcher->files());
    emit contextChanged();
}

void ContextManager::setDevAssManager(DevAssManager* manager)
{
    m_devAssManager = manager;
}

void ContextManager::setMainWindow(MainWindow* mainWindow)
{
    m_mainWindow = mainWindow;
}

QString ContextManager::generateUniqueId(const QString& commandName)
{
    return QString("%1_%2").arg(commandName).arg(++m_commandCounter);
}

void ContextManager::onFileChanged(const QString &path)
{
    // Small delay to allow file operations to complete
    QTimer::singleShot(100, this, [this, path]() {
        // Re-add the file to the watcher
        if (QFileInfo::exists(path)) {
            m_fileWatcher->addPath(path);
        }
        
        // Check which commands are affected by this file change
        for (auto it = m_commandHistory.begin(); it != m_commandHistory.end(); ++it) {
            CommandContextInfo& cmdInfo = it.value();
            
            // Check if this command involves the changed file
            QString cmdPath = cmdInfo.params.value("path").toString();
            if (!cmdPath.isEmpty()) {
                QFileInfo cmdFileInfo(cmdPath);
                QFileInfo changedFileInfo(path);
                
                if (cmdFileInfo.absoluteFilePath() == changedFileInfo.absoluteFilePath()) {
                    // Mark command as invalid due to file change
                    cmdInfo.isStillValid = false;
                    cmdInfo.invalidationReason = QString("File changed externally: %1").arg(path);
                    emit commandInvalidated(cmdInfo.id, cmdInfo.invalidationReason);
                }
            }
        }
        
        emit contextChanged();
    });
}

QDateTime ContextManager::getFileModificationTime(const QString& path) const
{
    QFileInfo fileInfo(path);
    if (fileInfo.exists()) {
        return fileInfo.lastModified();
    }
    return QDateTime(); // Invalid datetime
}