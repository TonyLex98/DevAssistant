#include "ContextManager.h"
#include "../command/Command.h"
#include "../core/DevAssManager.h"
#include "../ui/MainWindow.h"
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QQueue>
#include <QHash>
#include <algorithm>
#include "utils/Logging.h"

namespace {
// QSet::fromList is deprecated in Qt 5.15.
QSet<QString> toSet(const QStringList& list) { return QSet<QString>(list.begin(), list.end()); }
} // namespace

namespace {
constexpr int kMaxActiveContextChars = 18000;
constexpr int kMaxPerCommandChars = 3000;
constexpr int kMaxArchiveChunks = 60;
constexpr int kMaxExhumedChunks = 2;
constexpr double kTopicShiftThreshold = 0.12;
constexpr double kExhumeThreshold = 0.18;

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

static bool isTimeSensitiveNewsQuery(const QString& text)
{
    const QString lower = text.toLower();
    const bool looksLikeNews =
        lower.contains(QStringLiteral("\u043d\u043e\u0432\u043e\u0441\u0442")) ||
        lower.contains(QStringLiteral("\u0443\u043a\u0440\u0430\u0438\u043d")) ||
        lower.contains(QStringLiteral("\u0440\u043e\u0441\u0441\u0438")) ||
        lower.contains(QStringLiteral("news")) ||
        lower.contains(QStringLiteral("war"));
    if (!looksLikeNews)
        return false;

    return lower.contains(QStringLiteral("\u0441\u0435\u0433\u043e\u0434\u043d\u044f")) ||
           lower.contains(QStringLiteral("\u0441\u0435\u0439\u0447\u0430\u0441")) ||
           lower.contains(QStringLiteral("\u043f\u043e\u0441\u043b\u0435\u0434\u043d")) ||
           lower.contains(QStringLiteral("latest")) ||
           lower.contains(QStringLiteral("breaking")) ||
           lower.contains(QStringLiteral("current")) ||
           lower.contains(QStringLiteral("now"));
}
} // namespace

ContextManager::ContextManager(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_devAssManager(nullptr)
    , m_mainWindow(nullptr)
    , m_commandCounter(0)
    , m_sequenceCounter(0)
{
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &ContextManager::onFileChanged);

    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_archiveFilePath = ensureDirAndJoin(appDataDir, QStringLiteral("context_archive.json"));
    m_contextGraphFilePath = ensureDirAndJoin(appDataDir, QStringLiteral("context_graph.json"));
    loadArchive();
    loadContextGraph();
}

ContextManager::~ContextManager()
{
    // Cleanup is handled by parent-child relationship
    saveArchive();
    saveContextGraph();
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
        attachCommandToContextGraph(info);
        
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
                    attachCommandToContextGraph(info);
                    
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
    attachCommandToContextGraph(info);
    
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
    const QString graphContext = graphContextText(7, 4);
    if (!graphContext.trimmed().isEmpty())
        contextParts << graphContext;

    if (includeArchived && !m_exhumedArchiveIds.isEmpty() && !m_archive.isEmpty()) {
        QStringList blocks;
        blocks << QStringLiteral("=== RELEVANT ARCHIVED CONTEXT ===");
        int added = 0;
        for (const QString& id : m_exhumedArchiveIds) {
            auto it = std::find_if(m_archive.cbegin(), m_archive.cend(),
                                   [&](const ArchivedContextChunk& c){ return c.id == id; });
            if (it == m_archive.cend())
                continue;

            QString block = QStringLiteral("=== ARCHIVE: %1 (%2) ===")
                                .arg(it->title.isEmpty() ? QStringLiteral("Untitled") : it->title,
                                     it->timestamp.isValid() ? it->timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("?"));
            if (!it->reason.isEmpty())
                block += QStringLiteral("\nReason: %1").arg(it->reason);
            block += QStringLiteral("\n") + it->contextText.trimmed();

            if (block.size() > 6000)
                block = block.left(6000) + QStringLiteral("\n[...archived context truncated...]");

            blocks << block;
            if (++added >= kMaxExhumedChunks)
                break;
        }
        if (blocks.size() > 1)
            contextParts << blocks.join("\n\n");
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
    clearContextGraph();
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

QList<ContextGraphNodeInfo> ContextManager::getContextGraphNodes() const
{
    QList<ContextGraphNodeInfo> ordered;
    if (m_contextGraphNodes.isEmpty())
        return ordered;

    QSet<QString> visited;
    QQueue<QString> queue;
    QStringList roots = m_categoryRootNodeIds.values();
    roots.removeDuplicates();
    std::sort(roots.begin(), roots.end());
    for (const QString& rootId : roots) {
        if (m_contextGraphNodes.contains(rootId))
            queue.enqueue(rootId);
    }

    while (!queue.isEmpty()) {
        const QString id = queue.dequeue();
        if (visited.contains(id) || !m_contextGraphNodes.contains(id))
            continue;
        visited.insert(id);
        ordered << m_contextGraphNodes.value(id);

        QList<ContextGraphNodeInfo> children;
        for (const auto& node : m_contextGraphNodes) {
            if (node.parentId == id)
                children << node;
        }
        std::sort(children.begin(), children.end(), [](const ContextGraphNodeInfo& a, const ContextGraphNodeInfo& b) {
            if (a.updatedAt.isValid() && b.updatedAt.isValid() && a.updatedAt != b.updatedAt)
                return a.updatedAt < b.updatedAt;
            return a.id < b.id;
        });
        for (const auto& child : children)
            queue.enqueue(child.id);
    }

    QList<ContextGraphNodeInfo> remaining;
    for (auto it = m_contextGraphNodes.cbegin(); it != m_contextGraphNodes.cend(); ++it) {
        if (!visited.contains(it.key()))
            remaining << it.value();
    }
    std::sort(remaining.begin(), remaining.end(), [](const ContextGraphNodeInfo& a, const ContextGraphNodeInfo& b) {
        if (a.updatedAt.isValid() && b.updatedAt.isValid() && a.updatedAt != b.updatedAt)
            return a.updatedAt < b.updatedAt;
        return a.id < b.id;
    });
    ordered.append(remaining);
    return ordered;
}

QString ContextManager::normalizeCategory(const QString& category)
{
    const QString c = category.trimmed().toLower();
    if (c == QStringLiteral("project") || c == QStringLiteral("coding"))
        return QStringLiteral("project");
    if (c == QStringLiteral("news"))
        return QStringLiteral("news");
    if (c == QStringLiteral("search"))
        return QStringLiteral("search");
    if (c == QStringLiteral("system"))
        return QStringLiteral("system");
    return QStringLiteral("general");
}

QString ContextManager::defaultCategoryTitle(const QString& category)
{
    const QString c = normalizeCategory(category);
    if (c == QStringLiteral("project"))
        return QStringLiteral("Project and Coding");
    if (c == QStringLiteral("news"))
        return QStringLiteral("News and Events");
    if (c == QStringLiteral("search"))
        return QStringLiteral("Exploration and Search");
    if (c == QStringLiteral("system"))
        return QStringLiteral("System and Tooling");
    return QStringLiteral("General Context");
}

QString ContextManager::shortText(const QString& text, int maxChars)
{
    QString t = text.simplified();
    if (maxChars > 0 && t.size() > maxChars)
        t = t.left(maxChars) + QStringLiteral("...");
    return t;
}

QString ContextManager::inferCategoryFromText(const QString& text) const
{
    const QString lower = text.toLower();
    const bool looksLikeNews =
        lower.contains(QStringLiteral("\u043d\u043e\u0432\u043e\u0441\u0442")) ||
        lower.contains(QStringLiteral("\u0443\u043a\u0440\u0430\u0438\u043d")) ||
        lower.contains(QStringLiteral("\u0440\u043e\u0441\u0441\u0438")) ||
        lower.contains(QStringLiteral("latest")) ||
        lower.contains(QStringLiteral("breaking")) ||
        lower.contains(QStringLiteral("news"));
    if (looksLikeNews)
        return QStringLiteral("news");

    const bool looksLikeProject =
        lower.contains(QStringLiteral("\u043f\u0440\u043e\u0435\u043a\u0442")) ||
        lower.contains(QStringLiteral("\u043a\u043e\u0434")) ||
        lower.contains(QStringLiteral("\u0444\u0430\u0439\u043b")) ||
        lower.contains(QStringLiteral("\u043e\u0448\u0438\u0431\u043a")) ||
        lower.contains(QStringLiteral("bug")) ||
        lower.contains(QStringLiteral("refactor")) ||
        lower.contains(QStringLiteral("compile")) ||
        lower.contains(QStringLiteral("qt")) ||
        lower.contains(QStringLiteral("c++")) ||
        lower.contains(QStringLiteral("cpp")) ||
        lower.contains(QStringLiteral("python")) ||
        lower.contains(QStringLiteral("java"));
    if (looksLikeProject)
        return QStringLiteral("project");

    const bool looksLikeSystem =
        lower.contains(QStringLiteral("powershell")) ||
        lower.contains(QStringLiteral("terminal")) ||
        lower.contains(QStringLiteral("shell")) ||
        lower.contains(QStringLiteral("bash")) ||
        lower.contains(QStringLiteral("cmd")) ||
        lower.contains(QStringLiteral("git"));
    if (looksLikeSystem)
        return QStringLiteral("system");

    const bool looksLikeSearch =
        lower.contains(QStringLiteral("\u043d\u0430\u0439\u0434")) ||
        lower.contains(QStringLiteral("\u043f\u043e\u0438\u0441\u043a")) ||
        lower.contains(QStringLiteral("search")) ||
        lower.contains(QStringLiteral("google")) ||
        lower.contains(QStringLiteral("web"));
    if (looksLikeSearch)
        return QStringLiteral("search");

    return QStringLiteral("general");
}

QString ContextManager::inferCategoryForCommand(const CommandContextInfo& info) const
{
    const QString name = info.name.trimmed().toUpper();
    if (name == QStringLiteral("SEARCH_WEB")) {
        const QString q = info.params.value(QStringLiteral("query")).toString();
        const QString byQuery = inferCategoryFromText(q);
        if (byQuery == QStringLiteral("general"))
            return QStringLiteral("search");
        return byQuery;
    }

    if (name.startsWith(QStringLiteral("SEARCH_PROJECT")) ||
        name.startsWith(QStringLiteral("OPEN_FILE")) ||
        name == QStringLiteral("CREATE_FILE") ||
        name == QStringLiteral("REPLACE_IN_FILE") ||
        name == QStringLiteral("EDIT_IN_RANGE") ||
        name == QStringLiteral("APPEND_TO_FILE") ||
        name == QStringLiteral("INSERT_AT_LINE") ||
        name == QStringLiteral("APPLY_PATCH")) {
        return QStringLiteral("project");
    }

    if (name.startsWith(QStringLiteral("GIT_")) ||
        name == QStringLiteral("RUN_SHELL") ||
        name == QStringLiteral("RUN_PYTHON")) {
        return QStringLiteral("system");
    }

    if (name == QStringLiteral("SHOW_RESULT") || name == QStringLiteral("REASONING")) {
        if (!m_lastActiveGraphNodeId.isEmpty() && m_contextGraphNodes.contains(m_lastActiveGraphNodeId))
            return normalizeCategory(m_contextGraphNodes.value(m_lastActiveGraphNodeId).category);
        return QStringLiteral("general");
    }

    const QString fromText = inferCategoryFromText(info.description + QStringLiteral(" ") + info.result);
    return normalizeCategory(fromText);
}

QString ContextManager::ensureCategoryRootNode(const QString& category)
{
    const QString normalizedCategory = normalizeCategory(category);
    const QString existing = m_categoryRootNodeIds.value(normalizedCategory);
    if (!existing.isEmpty() && m_contextGraphNodes.contains(existing))
        return existing;

    const QString id = QStringLiteral("CAT_%1").arg(normalizedCategory);
    ContextGraphNodeInfo root;
    root.id = id;
    root.parentId.clear();
    root.category = normalizedCategory;
    root.title = defaultCategoryTitle(normalizedCategory);
    root.summary = QStringLiteral("Root branch for %1").arg(normalizedCategory);
    root.createdAt = QDateTime::currentDateTime();
    root.updatedAt = root.createdAt;
    root.characteristicTags = QStringList{normalizedCategory};
    root.milestone = true;

    m_contextGraphNodes[id] = root;
    m_categoryRootNodeIds[normalizedCategory] = id;
    return id;
}

QString ContextManager::createGraphNode(const QString& parentId,
                                        const QString& category,
                                        const QString& title,
                                        const QString& summary,
                                        const QStringList& tags,
                                        bool milestone)
{
    const QString normalizedCategory = normalizeCategory(category);
    const QString id = QStringLiteral("CGN_%1").arg(++m_graphNodeCounter);

    ContextGraphNodeInfo node;
    node.id = id;
    node.parentId = parentId;
    node.category = normalizedCategory;
    node.title = shortText(title, 72);
    node.summary = shortText(summary, 200);
    node.createdAt = QDateTime::currentDateTime();
    node.updatedAt = node.createdAt;
    node.characteristicTags = tags;
    node.characteristicTags.removeDuplicates();
    node.milestone = milestone;

    m_contextGraphNodes[id] = node;
    pruneContextGraph();
    return id;
}

void ContextManager::addActionToGraphNode(const QString& nodeId, const QString& action, bool milestone)
{
    if (nodeId.isEmpty() || !m_contextGraphNodes.contains(nodeId))
        return;

    ContextGraphNodeInfo node = m_contextGraphNodes.value(nodeId);
    const QString compactAction = shortText(action, 260);
    if (compactAction.isEmpty())
        return;

    node.actions << compactAction;
    while (node.actions.size() > 24)
        node.actions.removeFirst();
    node.updatedAt = QDateTime::currentDateTime();
    if (milestone)
        node.milestone = true;

    if (node.actions.size() > 12) {
        const QString movedAction = node.actions.takeLast();
        m_contextGraphNodes[nodeId] = node;

        const QString childId = createGraphNode(
            nodeId,
            node.category,
            QStringLiteral("%1 / branch").arg(shortText(node.title, 48)),
            QStringLiteral("Auto-created continuation branch"),
            node.characteristicTags,
            milestone);

        ContextGraphNodeInfo child = m_contextGraphNodes.value(childId);
        child.actions << movedAction;
        child.updatedAt = QDateTime::currentDateTime();
        child.milestone = child.milestone || milestone;
        m_contextGraphNodes[childId] = child;

        m_activeGraphNodeByCategory[node.category] = childId;
        m_lastActiveGraphNodeId = childId;
    } else {
        m_contextGraphNodes[nodeId] = node;
        m_activeGraphNodeByCategory[node.category] = nodeId;
        m_lastActiveGraphNodeId = nodeId;
    }

    pruneContextGraph();
    saveContextGraph();
}

void ContextManager::advanceContextGraphByUserQuery(const QString& text)
{
    const QString category = inferCategoryFromText(text);
    const QString rootId = ensureCategoryRootNode(category);
    QString activeId = m_activeGraphNodeByCategory.value(category);
    if (activeId.isEmpty() || !m_contextGraphNodes.contains(activeId))
        activeId = rootId;

    const QSet<QString> queryKeywords = extractKeywords(text);
    const QStringList tags = queryKeywords.values();

    if (activeId == rootId) {
        activeId = createGraphNode(rootId, category, shortText(text, 72), shortText(text, 200), tags, false);
    } else {
        ContextGraphNodeInfo active = m_contextGraphNodes.value(activeId);
        QSet<QString> nodeKeywords;
        for (const QString& tag : active.characteristicTags)
            nodeKeywords.insert(tag.toLower());
        const double sim = jaccardSimilarity(queryKeywords, nodeKeywords);
        if (sim < 0.25) {
            activeId = createGraphNode(active.id, category, shortText(text, 72), shortText(text, 200), tags, false);
        } else {
            const QString queryText = shortText(text, 120);
            if (!active.summary.contains(queryText))
                active.summary = shortText(active.summary + QStringLiteral(" | ") + queryText, 240);
            active.updatedAt = QDateTime::currentDateTime();
            for (const QString& t : tags) {
                if (!active.characteristicTags.contains(t))
                    active.characteristicTags << t;
            }
            while (active.characteristicTags.size() > 20)
                active.characteristicTags.removeFirst();
            m_contextGraphNodes[active.id] = active;
        }
    }

    m_activeGraphNodeByCategory[category] = activeId;
    m_lastActiveGraphNodeId = activeId;
    pruneContextGraph();
    saveContextGraph();
}

void ContextManager::attachCommandToContextGraph(const CommandContextInfo& info)
{
    const QString category = inferCategoryForCommand(info);
    const QString rootId = ensureCategoryRootNode(category);
    QString activeId = m_activeGraphNodeByCategory.value(category);
    if (activeId.isEmpty() || !m_contextGraphNodes.contains(activeId))
        activeId = createGraphNode(rootId,
                                   category,
                                   QStringLiteral("%1 thread").arg(defaultCategoryTitle(category)),
                                   shortText(info.description, 180),
                                   extractKeywords(info.description).values(),
                                   false);

    QString action = QStringLiteral("%1: %2")
                         .arg(info.name,
                              shortText(info.description.isEmpty() ? info.name : info.description, 140));
    if (!info.result.trimmed().isEmpty()) {
        const QStringList lines = info.result.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
            action += QStringLiteral(" -> %1").arg(shortText(lines.first(), 140));
    }

    const bool milestone =
        info.name == QStringLiteral("SHOW_RESULT") ||
        info.name == QStringLiteral("APPLY_PATCH") ||
        info.name == QStringLiteral("EDIT_IN_RANGE") ||
        info.name == QStringLiteral("REPLACE_IN_FILE");

    addActionToGraphNode(activeId, action, milestone);
    QString finalNodeId = m_activeGraphNodeByCategory.value(category);
    if (finalNodeId.isEmpty())
        finalNodeId = activeId;
    m_activeGraphNodeByCategory[category] = finalNodeId;
    if (!finalNodeId.isEmpty())
        m_lastActiveGraphNodeId = finalNodeId;
}

QString ContextManager::graphContextText(int maxNodes, int maxActionsPerNode) const
{
    if (m_contextGraphNodes.isEmpty())
        return QString();

    QString activeId = m_lastActiveGraphNodeId;
    if (activeId.isEmpty() || !m_contextGraphNodes.contains(activeId)) {
        for (auto it = m_activeGraphNodeByCategory.cbegin(); it != m_activeGraphNodeByCategory.cend(); ++it) {
            if (m_contextGraphNodes.contains(it.value())) {
                activeId = it.value();
                break;
            }
        }
    }
    if (activeId.isEmpty() || !m_contextGraphNodes.contains(activeId))
        return QString();

    QStringList chainIds;
    QSet<QString> guard;
    QString cursor = activeId;
    while (!cursor.isEmpty() && m_contextGraphNodes.contains(cursor) && !guard.contains(cursor)) {
        chainIds << cursor;
        guard.insert(cursor);
        cursor = m_contextGraphNodes.value(cursor).parentId;
    }
    std::reverse(chainIds.begin(), chainIds.end());
    if (maxNodes > 0 && chainIds.size() > maxNodes)
        chainIds = chainIds.mid(chainIds.size() - maxNodes);

    QStringList lines;
    lines << QStringLiteral("=== CONTEXT GRAPH ===");
    lines << QStringLiteral("Active node: %1 [%2]")
                 .arg(m_contextGraphNodes.value(activeId).title,
                      normalizeCategory(m_contextGraphNodes.value(activeId).category));
    lines << QStringLiteral("Branch:");

    for (const QString& id : chainIds) {
        if (!m_contextGraphNodes.contains(id))
            continue;
        const ContextGraphNodeInfo node = m_contextGraphNodes.value(id);
        lines << QStringLiteral("- %1 :: %2").arg(node.title, shortText(node.summary, 180));

        if (!node.actions.isEmpty()) {
            lines << QStringLiteral("  Actions:");
            const int total = node.actions.size();
            const int from = qMax(0, total - qMax(1, maxActionsPerNode));
            for (int i = from; i < total; ++i)
                lines << QStringLiteral("  * %1").arg(node.actions.at(i));
        }
    }

    return lines.join(QStringLiteral("\n"));
}

void ContextManager::pruneContextGraph()
{
    constexpr int kMaxGraphNodes = 260;
    if (m_contextGraphNodes.size() <= kMaxGraphNodes)
        return;

    QSet<QString> protectedIds;
    for (const QString& rootId : m_categoryRootNodeIds.values())
        protectedIds.insert(rootId);

    for (const QString& activeId : m_activeGraphNodeByCategory.values()) {
        QString cursor = activeId;
        QSet<QString> guard;
        while (!cursor.isEmpty() && m_contextGraphNodes.contains(cursor) && !guard.contains(cursor)) {
            protectedIds.insert(cursor);
            guard.insert(cursor);
            cursor = m_contextGraphNodes.value(cursor).parentId;
        }
    }

    while (m_contextGraphNodes.size() > kMaxGraphNodes) {
        QHash<QString, int> childrenCount;
        for (auto it = m_contextGraphNodes.cbegin(); it != m_contextGraphNodes.cend(); ++it) {
            const QString parent = it.value().parentId;
            if (!parent.isEmpty())
                childrenCount[parent] += 1;
        }

        QString candidateId;
        QDateTime oldest;
        for (auto it = m_contextGraphNodes.cbegin(); it != m_contextGraphNodes.cend(); ++it) {
            const QString id = it.key();
            if (protectedIds.contains(id))
                continue;
            if (childrenCount.value(id) > 0)
                continue;

            const QDateTime ts = it.value().updatedAt.isValid() ? it.value().updatedAt : it.value().createdAt;
            if (candidateId.isEmpty() || (ts.isValid() && (!oldest.isValid() || ts < oldest))) {
                candidateId = id;
                oldest = ts;
            }
        }

        if (candidateId.isEmpty())
            break;

        m_contextGraphNodes.remove(candidateId);
        for (auto it = m_activeGraphNodeByCategory.begin(); it != m_activeGraphNodeByCategory.end(); ++it) {
            if (it.value() == candidateId)
                it.value().clear();
        }
        if (m_lastActiveGraphNodeId == candidateId)
            m_lastActiveGraphNodeId.clear();
    }
}

void ContextManager::loadContextGraph()
{
    m_contextGraphNodes.clear();
    m_categoryRootNodeIds.clear();
    m_activeGraphNodeByCategory.clear();
    m_lastActiveGraphNodeId.clear();
    m_graphNodeCounter = 0;

    QFile f(m_contextGraphFilePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ensureCategoryRootNode(QStringLiteral("general"));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        ensureCategoryRootNode(QStringLiteral("general"));
        return;
    }

    const QJsonObject root = doc.object();
    m_graphNodeCounter = root.value(QStringLiteral("counter")).toInt(0);
    m_lastActiveGraphNodeId = root.value(QStringLiteral("lastActive")).toString();

    const QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
    for (const QJsonValue& v : nodes) {
        const QJsonObject o = v.toObject();
        ContextGraphNodeInfo node;
        node.id = o.value(QStringLiteral("id")).toString();
        node.parentId = o.value(QStringLiteral("parentId")).toString();
        node.category = normalizeCategory(o.value(QStringLiteral("category")).toString());
        node.title = o.value(QStringLiteral("title")).toString();
        node.summary = o.value(QStringLiteral("summary")).toString();
        node.createdAt = QDateTime::fromString(o.value(QStringLiteral("createdAt")).toString(), Qt::ISODate);
        node.updatedAt = QDateTime::fromString(o.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);
        node.milestone = o.value(QStringLiteral("milestone")).toBool(false);

        for (const QJsonValue& tv : o.value(QStringLiteral("tags")).toArray())
            node.characteristicTags << tv.toString();
        for (const QJsonValue& av : o.value(QStringLiteral("actions")).toArray())
            node.actions << av.toString();

        if (!node.id.isEmpty())
            m_contextGraphNodes.insert(node.id, node);
    }

    const QJsonObject roots = root.value(QStringLiteral("roots")).toObject();
    for (auto it = roots.begin(); it != roots.end(); ++it)
        m_categoryRootNodeIds[normalizeCategory(it.key())] = it.value().toString();

    const QJsonObject activeByCategory = root.value(QStringLiteral("activeByCategory")).toObject();
    for (auto it = activeByCategory.begin(); it != activeByCategory.end(); ++it)
        m_activeGraphNodeByCategory[normalizeCategory(it.key())] = it.value().toString();

    ensureCategoryRootNode(QStringLiteral("general"));
    pruneContextGraph();
}

void ContextManager::saveContextGraph() const
{
    if (m_contextGraphFilePath.isEmpty())
        return;

    QJsonArray nodes;
    for (const auto& node : m_contextGraphNodes) {
        QJsonObject o;
        o[QStringLiteral("id")] = node.id;
        o[QStringLiteral("parentId")] = node.parentId;
        o[QStringLiteral("category")] = normalizeCategory(node.category);
        o[QStringLiteral("title")] = node.title;
        o[QStringLiteral("summary")] = node.summary;
        o[QStringLiteral("createdAt")] = node.createdAt.isValid() ? node.createdAt.toString(Qt::ISODate) : QString();
        o[QStringLiteral("updatedAt")] = node.updatedAt.isValid() ? node.updatedAt.toString(Qt::ISODate) : QString();
        o[QStringLiteral("milestone")] = node.milestone;

        QJsonArray tags;
        for (const QString& tag : node.characteristicTags)
            tags.append(tag);
        o[QStringLiteral("tags")] = tags;

        QJsonArray actions;
        for (const QString& action : node.actions)
            actions.append(action);
        o[QStringLiteral("actions")] = actions;
        nodes.append(o);
    }

    QJsonObject roots;
    for (auto it = m_categoryRootNodeIds.cbegin(); it != m_categoryRootNodeIds.cend(); ++it)
        roots[it.key()] = it.value();

    QJsonObject activeByCategory;
    for (auto it = m_activeGraphNodeByCategory.cbegin(); it != m_activeGraphNodeByCategory.cend(); ++it)
        activeByCategory[it.key()] = it.value();

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("counter")] = m_graphNodeCounter;
    root[QStringLiteral("lastActive")] = m_lastActiveGraphNodeId;
    root[QStringLiteral("roots")] = roots;
    root[QStringLiteral("activeByCategory")] = activeByCategory;
    root[QStringLiteral("nodes")] = nodes;

    QSaveFile out(m_contextGraphFilePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logCore) << "[ContextManager] Failed to write context graph:" << m_contextGraphFilePath << out.errorString();
        return;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit())
        qCWarning(logCore) << "[ContextManager] Failed to commit context graph:" << m_contextGraphFilePath << out.errorString();
}

void ContextManager::clearContextGraph()
{
    m_contextGraphNodes.clear();
    m_categoryRootNodeIds.clear();
    m_activeGraphNodeByCategory.clear();
    m_lastActiveGraphNodeId.clear();
    m_graphNodeCounter = 0;
    ensureCategoryRootNode(QStringLiteral("general"));
    saveContextGraph();
}

void ContextManager::observeUserQuery(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    advanceContextGraphByUserQuery(trimmed);
    const bool timeSensitive = isTimeSensitiveNewsQuery(trimmed);

    const QSet<QString> newKeywords = extractKeywords(trimmed);
    if (newKeywords.isEmpty())
        return;

    if (m_currentTopicKeywords.isEmpty()) {
        m_currentTopicKeywords = newKeywords;
        m_currentTopicTitle = makeTopicTitle(trimmed);
        if (timeSensitive)
            m_exhumedArchiveIds.clear();
        updateExhumedArchivesForQuery(trimmed);
        return;
    }

    const double sim = jaccardSimilarity(m_currentTopicKeywords, newKeywords);
    const bool hasActiveContext = !getValidCommands().isEmpty() || !m_sequenceHistory.isEmpty();

    if (hasActiveContext && sim < kTopicShiftThreshold) {
        archiveAllActiveContext(QStringLiteral("Topic change detected"), trimmed);
        clearActiveContextOnly();
        m_currentTopicKeywords = newKeywords;
        m_currentTopicTitle = makeTopicTitle(trimmed);
        m_exhumedArchiveIds.clear();
    } else {
        QSet<QString> merged = m_currentTopicKeywords;
        for (const auto& k : newKeywords)
            merged.insert(k);
        if (merged.size() > 60) {
            QList<QString> list = merged.values();
            std::sort(list.begin(), list.end(), [](const QString& a, const QString& b) {
                if (a.size() != b.size())
                    return a.size() < b.size();
                return a < b;
            });
            while (list.size() > 60)
                list.removeFirst();
            merged = toSet(list);
        }
        m_currentTopicKeywords = merged;
    }

    if (timeSensitive)
        m_exhumedArchiveIds.clear();
    updateExhumedArchivesForQuery(trimmed);
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

void ContextManager::loadArchive()
{
    m_archive.clear();

    QFile f(m_archiveFilePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const QByteArray data = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return;

    const QJsonArray chunks = doc.object().value(QStringLiteral("chunks")).toArray();
    for (const QJsonValue& v : chunks) {
        const QJsonObject o = v.toObject();
        ArchivedContextChunk c;
        c.id = o.value(QStringLiteral("id")).toString();
        c.title = o.value(QStringLiteral("title")).toString();
        c.reason = o.value(QStringLiteral("reason")).toString();
        c.triggerQuery = o.value(QStringLiteral("triggerQuery")).toString();
        c.timestamp = QDateTime::fromString(o.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
        for (const QJsonValue& kw : o.value(QStringLiteral("keywords")).toArray())
            c.keywords << kw.toString();
        c.contextText = o.value(QStringLiteral("contextText")).toString();
        if (!c.id.isEmpty() && !c.contextText.trimmed().isEmpty())
            m_archive << c;
    }

    pruneArchive();
}

void ContextManager::saveArchive() const
{
    if (m_archiveFilePath.isEmpty())
        return;

    QJsonArray chunks;
    for (const auto& c : m_archive) {
        QJsonObject o;
        o[QStringLiteral("id")] = c.id;
        o[QStringLiteral("timestamp")] = c.timestamp.isValid() ? c.timestamp.toString(Qt::ISODate) : QString();
        o[QStringLiteral("title")] = c.title;
        o[QStringLiteral("reason")] = c.reason;
        o[QStringLiteral("triggerQuery")] = c.triggerQuery;
        QJsonArray kws;
        for (const auto& kw : c.keywords)
            kws.append(kw);
        o[QStringLiteral("keywords")] = kws;
        o[QStringLiteral("contextText")] = c.contextText;
        chunks.append(o);
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("chunks")] = chunks;

    QSaveFile out(m_archiveFilePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logCore) << "[ContextManager] Failed to write archive file:" << m_archiveFilePath << out.errorString();
        return;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit())
        qCWarning(logCore) << "[ContextManager] Failed to commit archive file:" << m_archiveFilePath << out.errorString();
}

void ContextManager::pruneArchive()
{
    if (m_archive.size() <= kMaxArchiveChunks)
        return;

    std::sort(m_archive.begin(), m_archive.end(), [](const ArchivedContextChunk& a, const ArchivedContextChunk& b) {
        return a.timestamp > b.timestamp;
    });
    while (m_archive.size() > kMaxArchiveChunks)
        m_archive.removeLast();
}

void ContextManager::archiveAllActiveContext(const QString& reason, const QString& triggerQuery)
{
    const QString ctx = generateContextTextInternal(false, 0).trimmed();
    if (ctx.isEmpty())
        return;

    ArchivedContextChunk chunk;
    chunk.id = QStringLiteral("ARCH_%1").arg(QString::number(QDateTime::currentMSecsSinceEpoch()));
    chunk.timestamp = QDateTime::currentDateTime();
    chunk.title = m_currentTopicTitle.trimmed().isEmpty() ? QStringLiteral("Archived context") : m_currentTopicTitle.trimmed();
    chunk.reason = reason;
    chunk.triggerQuery = triggerQuery;
    chunk.keywords = m_currentTopicKeywords.values();
    chunk.contextText = ctx;

    m_archive.prepend(chunk);
    pruneArchive();
    saveArchive();
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
            if (!archivedBlocks.isEmpty()) {
                ArchivedContextChunk chunk;
                chunk.id = QStringLiteral("ARCH_%1").arg(QString::number(QDateTime::currentMSecsSinceEpoch()));
                chunk.timestamp = QDateTime::currentDateTime();
                chunk.title = m_currentTopicTitle.trimmed().isEmpty()
                                  ? QStringLiteral("Archived context (compaction)")
                                  : m_currentTopicTitle.trimmed();
                chunk.reason = QStringLiteral("Context compaction (size limit)");
                chunk.keywords = m_currentTopicKeywords.values();
                chunk.contextText = archivedBlocks.join("\n\n");
                m_archive.prepend(chunk);
                pruneArchive();
                saveArchive();
            }
            emit contextChanged();
            return;
        }
    }

    emit contextChanged();
}

void ContextManager::updateExhumedArchivesForQuery(const QString& queryText)
{
    if (isTimeSensitiveNewsQuery(queryText)) {
        m_exhumedArchiveIds.clear();
        return;
    }

    if (m_archive.isEmpty()) {
        m_exhumedArchiveIds.clear();
        return;
    }

    const QSet<QString> qk = extractKeywords(queryText);
    if (qk.isEmpty()) {
        m_exhumedArchiveIds.clear();
        return;
    }

    struct Scored { QString id; double score; QDateTime ts; };
    QList<Scored> scored;
    scored.reserve(m_archive.size());

    for (const auto& c : m_archive) {
        QSet<QString> ck;
        for (const auto& kw : c.keywords)
            ck.insert(kw);
        const double sim = jaccardSimilarity(qk, ck);
        if (sim >= kExhumeThreshold)
            scored.push_back({c.id, sim, c.timestamp});
    }

    std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.ts > b.ts;
    });

    QStringList ids;
    for (int i = 0; i < scored.size() && i < kMaxExhumedChunks; ++i)
        ids << scored[i].id;

    m_exhumedArchiveIds = ids;
}

QSet<QString> ContextManager::extractKeywords(const QString& text)
{
    static const QSet<QString> stopwords = {
        // RU
        "и","в","во","на","по","к","ко","с","со","а","но","или","что","это","как","для","про","у","из","от","до","же","то","ли","не","да",
        // EN
        "the","a","an","and","or","to","of","in","on","for","with","is","are","was","were","be","been","as","at","by","from","that","this","it"
    };

    const QString lower = text.toLower();
    QRegularExpression re(QStringLiteral("[\\p{L}\\p{N}_]{3,}"));
    QRegularExpressionMatchIterator it = re.globalMatch(lower);

    QHash<QString, int> freq;
    while (it.hasNext()) {
        const QString token = it.next().captured(0);
        if (stopwords.contains(token))
            continue;
        freq[token]++;
    }

    QList<QPair<QString, int>> items;
    items.reserve(freq.size());
    for (auto f = freq.cbegin(); f != freq.cend(); ++f)
        items.push_back({f.key(), f.value()});

    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second)
            return a.second > b.second;
        return a.first < b.first;
    });

    QSet<QString> out;
    const int limit = std::min(30, items.size());
    for (int i = 0; i < limit; ++i)
        out.insert(items[i].first);
    return out;
}

double ContextManager::jaccardSimilarity(const QSet<QString>& a, const QSet<QString>& b)
{
    if (a.isEmpty() || b.isEmpty())
        return 0.0;

    int inter = 0;
    for (const auto& k : a) {
        if (b.contains(k))
            ++inter;
    }
    const int uni = a.size() + b.size() - inter;
    return uni > 0 ? static_cast<double>(inter) / static_cast<double>(uni) : 0.0;
}

QString ContextManager::makeTopicTitle(const QString& text)
{
    QString t = text.trimmed();
    if (t.size() > 80)
        t = t.left(80) + QStringLiteral("...");
    return t;
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
        attachCommandToContextGraph(info);
        
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
                    attachCommandToContextGraph(info);
                    
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
    attachCommandToContextGraph(info);

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
