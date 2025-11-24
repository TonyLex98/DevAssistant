#include "ContextManager.h"
#include "../command/Command.h"
#include "../core/DevAssManager.h"
#include "../ui/MainWindow.h"
#include <QDateTime>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <algorithm>

ContextManager::ContextManager(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_devAssManager(nullptr)
    , m_mainWindow(nullptr)
    , m_commandCounter(0)
    , m_sequenceCounter(0)
{
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &ContextManager::onFileChanged);
}

ContextManager::~ContextManager()
{
    // Cleanup is handled by parent-child relationship
}

QString ContextManager::findExistingCommandId(const QString& commandName, const QJsonObject& params) const
{
    for (auto it = m_commandHistory.constBegin(); it != m_commandHistory.constEnd(); ++it) {
        const CommandContextInfo& info = it.value();
        if (info.name == commandName && info.params == params && info.state == CommandState::Active) {
            return info.id;
        }
    }
    return QString(); // Not found
}

static QString normalizePath(const QString& path, DevAssManager* devAss)
{
    if (path.trimmed().isEmpty())
        return QString();

    QFileInfo info(path);
    if (info.isAbsolute())
        return QDir::cleanPath(info.absoluteFilePath());

    if (devAss) {
        return QDir::cleanPath(devAss->projectRoot().absoluteFilePath(path));
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QStringList ContextManager::collectPathsFromParams(const QJsonObject& params) const
{
    static const QStringList keys = {
        "path",
        "oldPath",
        "newPath",
        "scriptPath",
        "folderPath",
        "sourceFilePath"
    };

    QStringList paths;
    for (const QString& key : keys) {
        const QString raw = params.value(key).toString();
        const QString normalized = normalizePath(raw, m_devAssManager);
        if (!normalized.isEmpty()) {
            paths << normalized;
        }
    }
    paths.removeDuplicates();
    return paths;
}

void ContextManager::addPathsToWatcher(const QStringList& paths)
{
    if (!m_fileWatcher)
        return;

    QStringList existing = m_fileWatcher->files();
    for (const QString& path : paths) {
        QFileInfo fileInfo(path);
        if (fileInfo.exists() && !existing.contains(fileInfo.absoluteFilePath())) {
            m_fileWatcher->addPath(fileInfo.absoluteFilePath());
        }
    }
}

void ContextManager::addCommand(Command* cmd, const QString& result)
{
    if (!cmd) return;
    
    // Check if command with same params already exists and is in Active state
    // Only update if there's an existing active command with same params
    QString existingActiveId = findExistingCommandId(cmd->name(), cmd->params());
    if (!existingActiveId.isEmpty()) {
        // Update existing active command
        CommandContextInfo& info = m_commandHistory[existingActiveId];
        info.timestamp = QDateTime::currentDateTime();
        info.result = result;
        info.state = CommandState::Active; // Reset to active
        info.reason.clear(); // Clear any previous reason
        info.affectedPaths = collectPathsFromParams(info.params);
        addPathsToWatcher(info.affectedPaths);
        
        emit commandAdded(existingActiveId);
        emit contextChanged();
        return;
    } else {
        // Look for any command with same name and params regardless of state
        // If found, we might want to check if we should create a new one based on its state
        for (auto it = m_commandHistory.begin(); it != m_commandHistory.end(); ++it) {
            CommandContextInfo& info = it.value();
            if (info.name == cmd->name() && info.params == cmd->params()) {
                // If the existing command is invalidated or removed, we should create a new one
                if (info.state != CommandState::Active) {
                    break; // Will create a new command below
                } else {
                    // This shouldn't happen due to findExistingCommandId check above, but just in case
                    info.timestamp = QDateTime::currentDateTime();
                    info.result = result;
                    info.state = CommandState::Active; // Reset to active
                    info.reason.clear(); // Clear any previous reason
                    info.affectedPaths = collectPathsFromParams(info.params);
                    addPathsToWatcher(info.affectedPaths);
                    
                    emit commandAdded(it.key());
                    emit contextChanged();
                    return;
                }
            }
        }
    }
    
    // Create new command
    QString uniqueId = generateUniqueId(cmd->name());
    CommandContextInfo info;
    info.id = uniqueId;
    info.name = cmd->name();
    info.params = cmd->params();
    info.description = cmd->description();
    info.timestamp = QDateTime::currentDateTime();
    info.result = result;
    info.state = CommandState::Active;
    info.affectedPaths = collectPathsFromParams(info.params);
    
    m_commandHistory.insert(uniqueId, info);
    
    addPathsToWatcher(info.affectedPaths);
    
    emit commandAdded(uniqueId);
    emit contextChanged();
}

void ContextManager::invalidateCommand(const QString& commandId, const QString& reason)
{
    if (m_commandHistory.contains(commandId)) {
        CommandContextInfo& info = m_commandHistory[commandId];
        info.state = CommandState::Invalidated;
        info.reason = reason;
        
        emit commandInvalidated(commandId, reason);
        emit contextChanged();
    }
}

void ContextManager::removeCommand(const QString& commandId)
{
    if (m_commandHistory.remove(commandId) > 0) {
        // Remove the command ID from any sequence it belonged to
        for (auto it = m_sequenceHistory.begin(); it != m_sequenceHistory.end(); ++it) {
            it.value().commandIds.removeAll(commandId);
        }
        cleanupEmptySequences();
        rebuildFileWatcher();
        emit commandRemoved(commandId);
        emit contextChanged();
    }
}

QList<CommandContextInfo> ContextManager::getValidCommands() const
{
    QList<CommandContextInfo> validCommands;
    for (auto it = m_commandHistory.constBegin(); it != m_commandHistory.constEnd(); ++it) {
        if (it.value().state == CommandState::Active) {
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
    
    // Get all sequences to organize commands by sequence
    QList<CommandSequenceInfo> allSequences = getCommandSequences();
    
    if (allSequences.isEmpty()) {
        // If no sequences, fall back to showing individual commands as before
        for (const auto& cmdInfo : getValidCommands()) {
            if (!cmdInfo.result.isEmpty()) {
                contextParts << cmdInfo.result;
            }
        }
    } else {
        // Organize commands by sequences
        for (const auto& seqInfo : allSequences) {
            // Add sequence header
            QStringList seqContent;
            seqContent << QString("=== SEQUENCE: %1 ===").arg(seqInfo.name);
            if (!seqInfo.description.isEmpty()) {
                seqContent << seqInfo.description;
            }
            
            // Add commands in this sequence
            QList<CommandContextInfo> commandsInSequence = getCommandsInSequence(seqInfo.id);
            for (const auto& cmdInfo : commandsInSequence) {
                if (!cmdInfo.result.isEmpty() && cmdInfo.state == CommandState::Active) {
                    seqContent << cmdInfo.result;
                }
            }
            
            if (seqContent.size() > 2) { // If there are commands beyond the header
                contextParts << seqContent.join("\n");
            }
        }
    }
    
    return contextParts.join("\n\n");
}

void ContextManager::clearAll()
{
    m_commandHistory.clear();
    m_sequenceHistory.clear();
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

void ContextManager::cleanupEmptySequences()
{
    QList<QString> emptyIds;
    for (auto it = m_sequenceHistory.constBegin(); it != m_sequenceHistory.constEnd(); ++it) {
        if (it.value().commandIds.isEmpty()) {
            emptyIds << it.key();
        }
    }
    for (const QString& id : emptyIds) {
        m_sequenceHistory.remove(id);
    }
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
        
        cleanupForPath(path, QString("File changed externally: %1").arg(path));
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

QString ContextManager::startCommandSequence(const QString& name, const QString& description)
{
    QString sequenceId = QString("SEQ_%1").arg(++m_sequenceCounter);
    
    CommandSequenceInfo seqInfo;
    seqInfo.id = sequenceId;
    seqInfo.name = name.isEmpty() ? QString("Sequence %1").arg(m_sequenceCounter) : name;
    seqInfo.description = description;
    seqInfo.timestamp = QDateTime::currentDateTime();
    
    m_sequenceHistory.insert(sequenceId, seqInfo);
    
    emit sequenceStarted(sequenceId);
    emit contextChanged();
    
    return sequenceId;
}

void ContextManager::endCommandSequence(const QString& sequenceId)
{
    if (m_sequenceHistory.contains(sequenceId)) {
        CommandSequenceInfo& seqInfo = m_sequenceHistory[sequenceId];
        // Optionally update timestamp or add end time here
        emit sequenceEnded(sequenceId);
        emit contextChanged();
    }
}

void ContextManager::addCommandToSequence(Command* cmd, const QString& sequenceId, const QString& result)
{
    if (!cmd || !m_sequenceHistory.contains(sequenceId)) return;
    
    // Check if command with same params already exists and is in Active state
    QString existingActiveId = findExistingCommandId(cmd->name(), cmd->params());
    if (!existingActiveId.isEmpty()) {
        // Update existing active command with sequence info
        CommandContextInfo& info = m_commandHistory[existingActiveId];
        info.timestamp = QDateTime::currentDateTime();
        info.result = result;
        info.sequenceId = sequenceId; // Link to the sequence
        info.state = CommandState::Active; // Reset to active
        info.reason.clear(); // Clear any previous reason
        
        // Also add to the sequence if not already there
        CommandSequenceInfo& seqInfo = m_sequenceHistory[sequenceId];
        if (!seqInfo.commandIds.contains(existingActiveId)) {
            seqInfo.commandIds.append(existingActiveId);
        }
        
        emit commandAddedToSequence(existingActiveId, sequenceId);
        emit commandAdded(existingActiveId);
        emit contextChanged();
        return;
    } else {
        // Look for any command with same name and params regardless of state
        // If found, we might want to check if we should create a new one based on its state
        for (auto it = m_commandHistory.begin(); it != m_commandHistory.end(); ++it) {
            CommandContextInfo& info = it.value();
            if (info.name == cmd->name() && info.params == cmd->params()) {
                // If the existing command is invalidated or removed, we should create a new one
                if (info.state != CommandState::Active) {
                    break; // Will create a new command below
                } else {
                    // This shouldn't happen due to findExistingCommandId check above, but just in case
                    info.timestamp = QDateTime::currentDateTime();
                    info.result = result;
                    info.sequenceId = sequenceId; // Link to the sequence
                    info.state = CommandState::Active; // Reset to active
                    info.reason.clear(); // Clear any previous reason
                    
                    // Also add to the sequence if not already there
                    CommandSequenceInfo& seqInfo = m_sequenceHistory[sequenceId];
                    if (!seqInfo.commandIds.contains(it.key())) {
                        seqInfo.commandIds.append(it.key());
                    }
                    
                    emit commandAddedToSequence(it.key(), sequenceId);
                    emit commandAdded(it.key());
                    emit contextChanged();
                    return;
                }
            }
        }
    }
    
    // First add the command to the main command history with sequence info
    QString commandId = generateUniqueId(cmd->name());
    CommandContextInfo info;
    info.id = commandId;
    info.name = cmd->name();
    info.params = cmd->params();
    info.description = cmd->description();
    info.timestamp = QDateTime::currentDateTime();
    info.result = result;
    info.state = CommandState::Active;
    info.sequenceId = sequenceId; // Link to the sequence

    m_commandHistory.insert(commandId, info);
    
    addPathsToWatcher(info.affectedPaths);

    // Add command ID to the sequence
    CommandSequenceInfo& seqInfo = m_sequenceHistory[sequenceId];
    seqInfo.commandIds.append(commandId);
    
    emit commandAddedToSequence(commandId, sequenceId);
    emit commandAdded(commandId);
    emit contextChanged();
}

QList<CommandSequenceInfo> ContextManager::getCommandSequences() const
{
    QList<CommandSequenceInfo> sequences;
    for (auto it = m_sequenceHistory.constBegin(); it != m_sequenceHistory.constEnd(); ++it) {
        sequences.append(it.value());
    }
    return sequences;
}

QList<CommandContextInfo> ContextManager::getCommandsInSequence(const QString& sequenceId) const
{
    QList<CommandContextInfo> commands;
    if (m_sequenceHistory.contains(sequenceId)) {
        const CommandSequenceInfo& seqInfo = m_sequenceHistory[sequenceId];
        for (const QString& commandId : seqInfo.commandIds) {
            if (m_commandHistory.contains(commandId)) {
                commands.append(m_commandHistory[commandId]);
            }
        }
    }
    return commands;
}

CommandSequenceInfo ContextManager::getSequenceInfo(const QString& sequenceId) const
{
    if (m_sequenceHistory.contains(sequenceId)) {
        return m_sequenceHistory[sequenceId];
    }
    return CommandSequenceInfo(); // Return empty sequence info if not found
}

void ContextManager::cleanupForPath(const QString& path, const QString& reason)
{
    const QString normalizedPath = normalizePath(path, m_devAssManager);
    QStringList toRemove;

    for (auto it = m_commandHistory.begin(); it != m_commandHistory.end(); ++it) {
        CommandContextInfo& cmdInfo = it.value();
        const bool touchesFile = std::any_of(cmdInfo.affectedPaths.constBegin(),
                                             cmdInfo.affectedPaths.constEnd(),
                                             [&](const QString& p){
                                                 return QFileInfo(p).absoluteFilePath() == normalizedPath;
                                             });
        if (!touchesFile)
            continue;

        if (cmdInfo.state == CommandState::Active) {
            cmdInfo.state = CommandState::Invalidated;
            cmdInfo.reason = reason.isEmpty()
                ? QString("Context invalidated because %1 changed").arg(normalizedPath)
                : reason;
            emit commandInvalidated(cmdInfo.id, cmdInfo.reason);
        }
        toRemove << cmdInfo.id;
    }

    for (const QString& id : toRemove) {
        removeCommand(id);
    }

    emit contextChanged();
}

void ContextManager::rebuildFileWatcher()
{
    if (!m_fileWatcher)
        return;

    const QStringList current = m_fileWatcher->files();
    if (!current.isEmpty())
        m_fileWatcher->removePaths(current);

    QSet<QString> paths;
    for (auto it = m_commandHistory.constBegin(); it != m_commandHistory.constEnd(); ++it) {
        for (const QString& path : it.value().affectedPaths) {
            QFileInfo info(path);
            if (info.exists()) {
                paths.insert(info.absoluteFilePath());
            }
        }
    }

    if (!paths.isEmpty()) {
        m_fileWatcher->addPaths(paths.values());
    }
}
