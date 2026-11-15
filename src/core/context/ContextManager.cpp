#include "core/context/ContextManager.h"
#include "core/context/ContextKeywords.h"
#include "core/commands/Command.h"
#include "core/project/DevAssManager.h"
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QRegularExpression>
#include <algorithm>

namespace {
constexpr int kMaxActiveContextChars = 18000;
constexpr int kMaxPerCommandChars = 3000;

static int extractTrailingNumber(const QString& id)
{
    QRegularExpression re(QStringLiteral("(\\d+)$"));
    const QRegularExpressionMatch m = re.match(id);
    if (!m.hasMatch())
        return -1;
    return m.captured(1).toInt();
}

static QString ensureDirAndJoin(const QString& dirPath, const QString& fileName)
{
    QDir dir(dirPath);
    if (!dir.exists())
        dir.mkpath(".");
    return dir.filePath(fileName);
}

static QString jsonCompact(const QJsonObject& obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
} // namespace

ContextManager::ContextManager(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_devAssManager(nullptr)
    , m_commandCounter(0)
    , m_sequenceCounter(0)
{
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &ContextManager::onFileChanged);

    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString archiveFilePath = ensureDirAndJoin(appDataDir, QStringLiteral("context_archive.json"));
    const QString graphFilePath = ensureDirAndJoin(appDataDir, QStringLiteral("context_graph.json"));
    m_archive.load(archiveFilePath);
    m_graph.load(graphFilePath);
}

ContextManager::~ContextManager()
{
    // Cleanup is handled by parent-child relationship
    m_archive.save();
    m_graph.save();
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
        info.result = result;
        info.state = CommandState::Active; // Reset to active
        info.reason.clear(); // Clear any previous reason
        info.affectedPaths = collectPathsFromParams(info.params);
        addPathsToWatcher(info.affectedPaths);
        m_graph.attachCommand(info);
        
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
                    info.result = result;
                    info.state = CommandState::Active; // Reset to active
                    info.reason.clear(); // Clear any previous reason
                    info.affectedPaths = collectPathsFromParams(info.params);
                    addPathsToWatcher(info.affectedPaths);
                    m_graph.attachCommand(info);
                    
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
    m_graph.attachCommand(info);
    
    emit commandAdded(uniqueId);
    emit contextChanged();

    archiveOldestUntilFit(kMaxActiveContextChars);
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
    std::sort(validCommands.begin(), validCommands.end(), [](const CommandContextInfo& a, const CommandContextInfo& b) {
        const int an = extractTrailingNumber(a.id);
        const int bn = extractTrailingNumber(b.id);
        if (an != -1 && bn != -1 && an != bn)
            return an < bn;
        if (a.timestamp.isValid() && b.timestamp.isValid() && a.timestamp != b.timestamp)
            return a.timestamp < b.timestamp;
        return a.id < b.id;
    });
    return validCommands;
}

QList<CommandContextInfo> ContextManager::getAllCommands() const
{
    QList<CommandContextInfo> allCommands;
    for (auto it = m_commandHistory.constBegin(); it != m_commandHistory.constEnd(); ++it) {
        allCommands.append(it.value());
    }
    std::sort(allCommands.begin(), allCommands.end(), [](const CommandContextInfo& a, const CommandContextInfo& b) {
        const int an = extractTrailingNumber(a.id);
        const int bn = extractTrailingNumber(b.id);
        if (an != -1 && bn != -1 && an != bn)
            return an < bn;
        if (a.timestamp.isValid() && b.timestamp.isValid() && a.timestamp != b.timestamp)
            return a.timestamp < b.timestamp;
        return a.id < b.id;
    });
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
    return generateContextTextInternal(true, kMaxActiveContextChars);
}

QString ContextManager::generateContextTextInternal(bool includeArchived, int maxChars) const
{
    QStringList contextParts;
    const QString graphContext = m_graph.contextText(7, 4);
    if (!graphContext.trimmed().isEmpty())
        contextParts << graphContext;

    if (includeArchived) {
        const QString archived = m_archive.exhumedContextText();
        if (!archived.isEmpty())
            contextParts << archived;
    }

    QList<CommandSequenceInfo> sequences = getCommandSequences();
    std::sort(sequences.begin(), sequences.end(), [](const CommandSequenceInfo& a, const CommandSequenceInfo& b) {
        return a.timestamp < b.timestamp;
    });

    for (const auto& seqInfo : sequences) {
        const QString seqBlock = formatSequenceForContext(seqInfo, kMaxPerCommandChars);
        if (!seqBlock.trimmed().isEmpty())
            contextParts << seqBlock;
    }

    QList<CommandContextInfo> standalone;
    for (const auto& cmd : getValidCommands()) {
        if (cmd.sequenceId.isEmpty())
            standalone << cmd;
    }
    std::sort(standalone.begin(), standalone.end(), [](const CommandContextInfo& a, const CommandContextInfo& b) {
        return a.timestamp < b.timestamp;
    });
    if (!standalone.isEmpty()) {
        QStringList blocks;
        blocks << QStringLiteral("=== COMMANDS (standalone) ===");
        for (const auto& cmd : standalone) {
            const QString formatted = formatCommandForContext(cmd, kMaxPerCommandChars);
            if (!formatted.trimmed().isEmpty())
                blocks << formatted;
        }
        if (blocks.size() > 1)
            contextParts << blocks.join("\n\n");
    }

    QString out = contextParts.join("\n\n").trimmed();
    if (maxChars > 0 && out.size() > maxChars) {
        out = out.right(maxChars);
        out.prepend(QStringLiteral("[...context truncated to last %1 chars...]\n").arg(maxChars));
    }
    return out;
}

QString ContextManager::formatSequenceForContext(const CommandSequenceInfo& seqInfo, int maxCharsPerCommand) const
{
    QList<CommandContextInfo> commandsInSequence = getCommandsInSequence(seqInfo.id);
    if (commandsInSequence.isEmpty())
        return QString();

    QStringList blocks;
    blocks << QStringLiteral("=== SEQUENCE: %1 ===").arg(seqInfo.name.isEmpty() ? seqInfo.id : seqInfo.name);
    if (!seqInfo.description.trimmed().isEmpty())
        blocks << QStringLiteral("Description: %1").arg(seqInfo.description.trimmed());

    int added = 0;
    for (const auto& cmdInfo : commandsInSequence) {
        if (cmdInfo.state != CommandState::Active)
            continue;
        const QString formatted = formatCommandForContext(cmdInfo, maxCharsPerCommand);
        if (formatted.trimmed().isEmpty())
            continue;
        blocks << formatted;
        ++added;
    }

    if (added == 0)
        return QString();

    return blocks.join("\n\n");
}

QString ContextManager::formatCommandForContext(const CommandContextInfo& cmdInfo, int maxChars) const
{
    QString body = cmdInfo.result.trimmed();
    if (body.isEmpty())
        body = cmdInfo.description.trimmed();
    if (body.isEmpty())
        return QString();

    QStringList out;
    out << QStringLiteral("==== COMMAND: %1 ====").arg(cmdInfo.name);
    if (!cmdInfo.description.trimmed().isEmpty())
        out << QStringLiteral("Description: %1").arg(cmdInfo.description.trimmed());
    if (!cmdInfo.params.isEmpty())
        out << QStringLiteral("Params: %1").arg(jsonCompact(cmdInfo.params));
    out << QStringLiteral("Result:\n%1").arg(body);

    QString result = out.join("\n");
    if (maxChars > 0 && result.size() > maxChars) {
        result = result.left(maxChars) + QStringLiteral("\n[...command result truncated...]");
    }
    return result;
}

void ContextManager::clearAll()
{
    m_commandHistory.clear();
    m_sequenceHistory.clear();
    m_graph.clear();
    m_fileWatcher->removePaths(m_fileWatcher->files());
    emit contextChanged();
}

void ContextManager::setDevAssManager(DevAssManager* manager)
{
    m_devAssManager = manager;
}

void ContextManager::observeUserQuery(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    m_graph.advanceByUserQuery(trimmed);

    const QSet<QString> keywords = ContextKeywords::extract(trimmed);
    if (keywords.isEmpty())
        return;

    if (!m_archive.hasTopic()) {
        m_archive.startTopic(trimmed, keywords);
    } else {
        const bool hasActiveContext = !getValidCommands().isEmpty() || !m_sequenceHistory.isEmpty();
        if (hasActiveContext && m_archive.isTopicShift(keywords)) {
            // Archived before the topic changes, so the chunk is labeled with the topic it belongs to.
            m_archive.store(generateContextTextInternal(false, 0).trimmed(),
                            QStringLiteral("Archived context"),
                            QStringLiteral("Topic change detected"),
                            trimmed);
            clearActiveContextOnly();
            m_archive.startTopic(trimmed, keywords);
        } else {
            m_archive.extendTopic(keywords);
        }
    }

    m_archive.updateExhumed(trimmed);
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

void ContextManager::clearActiveContextOnly()
{
    m_commandHistory.clear();
    m_sequenceHistory.clear();
    rebuildFileWatcher();
    emit contextChanged();
}

void ContextManager::archiveOldestUntilFit(int maxActiveChars)
{
    if (maxActiveChars <= 0)
        return;

    const QString current = generateContextTextInternal(false, 0);
    if (current.size() <= maxActiveChars)
        return;

    QList<CommandSequenceInfo> sequences = getCommandSequences();
    std::sort(sequences.begin(), sequences.end(), [](const CommandSequenceInfo& a, const CommandSequenceInfo& b) {
        return a.timestamp < b.timestamp;
    });
    if (sequences.isEmpty())
        return;

    QStringList archivedBlocks;

    for (const auto& seq : sequences) {
        const QString block = formatSequenceForContext(seq, kMaxPerCommandChars);
        if (block.trimmed().isEmpty())
            continue;

        archivedBlocks << block;

        if (m_sequenceHistory.contains(seq.id)) {
            const auto ids = m_sequenceHistory[seq.id].commandIds;
            for (const auto& cid : ids)
                m_commandHistory.remove(cid);
            m_sequenceHistory.remove(seq.id);
        }

        cleanupEmptySequences();
        rebuildFileWatcher();

        const QString after = generateContextTextInternal(false, 0);
        if (after.size() <= maxActiveChars) {
            m_archive.store(archivedBlocks.join("\n\n"),
                            QStringLiteral("Archived context (compaction)"),
                            QStringLiteral("Context compaction (size limit)"));
            emit contextChanged();
            return;
        }
    }

    emit contextChanged();
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
        info.result = result;
        // A command belongs to one sequence: take it out of the one it was in before.
        if (info.sequenceId != sequenceId && m_sequenceHistory.contains(info.sequenceId))
            m_sequenceHistory[info.sequenceId].commandIds.removeAll(existingActiveId);
        info.sequenceId = sequenceId; // Link to the sequence
        info.state = CommandState::Active; // Reset to active
        info.reason.clear(); // Clear any previous reason
        info.affectedPaths = collectPathsFromParams(info.params);
        addPathsToWatcher(info.affectedPaths);
        m_graph.attachCommand(info);
        
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
                    info.result = result;
                    info.sequenceId = sequenceId; // Link to the sequence
                    info.state = CommandState::Active; // Reset to active
                    info.reason.clear(); // Clear any previous reason
                    info.affectedPaths = collectPathsFromParams(info.params);
                    addPathsToWatcher(info.affectedPaths);
                    m_graph.attachCommand(info);
                    
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
    info.affectedPaths = collectPathsFromParams(info.params);

    m_commandHistory.insert(commandId, info);
    
    addPathsToWatcher(info.affectedPaths);
    m_graph.attachCommand(info);

    // Add command ID to the sequence
    CommandSequenceInfo& seqInfo = m_sequenceHistory[sequenceId];
    seqInfo.commandIds.append(commandId);
    
    emit commandAddedToSequence(commandId, sequenceId);
    emit commandAdded(commandId);
    emit contextChanged();

    archiveOldestUntilFit(kMaxActiveContextChars);
}

QList<CommandSequenceInfo> ContextManager::getCommandSequences() const
{
    QList<CommandSequenceInfo> sequences;
    for (auto it = m_sequenceHistory.constBegin(); it != m_sequenceHistory.constEnd(); ++it) {
        sequences.append(it.value());
    }
    std::sort(sequences.begin(), sequences.end(), [](const CommandSequenceInfo& a, const CommandSequenceInfo& b) {
        const int an = extractTrailingNumber(a.id);
        const int bn = extractTrailingNumber(b.id);
        if (an != -1 && bn != -1 && an != bn)
            return an < bn;
        if (a.timestamp.isValid() && b.timestamp.isValid() && a.timestamp != b.timestamp)
            return a.timestamp < b.timestamp;
        return a.id < b.id;
    });
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
