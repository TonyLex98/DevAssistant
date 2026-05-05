#ifndef CONTEXTMANAGER_H
#define CONTEXTMANAGER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QJsonObject>
#include <QStringList>
#include <QJsonArray>
#include <QStandardPaths>

// Forward declarations
class Command;
class DevAssManager;
class MainWindow;

// Enum for command states
enum class CommandState {
    Active,      // Command executed and still in context
    Invalidated, // Command executed but invalidated due to file changes, etc.
    Removed      // Command was removed from context
};

// Structure to track command context state
struct CommandContextInfo {
    QString id;
    QString name;
    QJsonObject params;
    QString description;
    QDateTime timestamp;
    CommandState state;          // Current state of the command
    QString reason;              // Reason for state (invalidation reason, etc.)
    QString result;              // Command result/output
    QString sequenceId;          // ID of the command sequence this command belongs to
    QStringList affectedPaths;   // Normalized file/folder paths the command touches
    
    CommandContextInfo() : state(CommandState::Active) {}
};

// Structure to track command sequences
struct CommandSequenceInfo {
    QString id;
    QString name;
    QDateTime timestamp;
    QString description;
    QList<QString> commandIds;   // IDs of commands in this sequence
    
    CommandSequenceInfo() {}
};

// Archived context chunk (for topic changes / context compaction).
struct ArchivedContextChunk {
    QString id;
    QDateTime timestamp;
    QString title;
    QString reason;
    QString triggerQuery;
    QStringList keywords;
    QString contextText;
};

// Graph node for hierarchical context memory.
struct ContextGraphNodeInfo {
    QString id;
    QString parentId;
    QString category;
    QString title;
    QString summary;
    QDateTime createdAt;
    QDateTime updatedAt;
    QStringList characteristicTags;
    QStringList actions;
    bool milestone = false;
};

class ContextManager : public QObject
{
    Q_OBJECT

public:
    explicit ContextManager(QObject *parent = nullptr);
    ~ContextManager();

    // Add a command to context tracking
    void addCommand(Command* cmd, const QString& result = QString());
    
    // Check if command with same params already exists (and is valid)
    QString findExistingCommandId(const QString& commandName, const QJsonObject& params) const;
    
    // Mark a command as invalidated due to file changes or other reasons
    void invalidateCommand(const QString& commandId, const QString& reason);
    
    // Remove command from tracking
    void removeCommand(const QString& commandId);
    
    // Get current valid context commands
    QList<CommandContextInfo> getValidCommands() const;
    
    // Get all commands (including invalidated)
    QList<CommandContextInfo> getAllCommands() const;
    
    // Check if a file has been modified since a command was executed
    bool isFileModifiedSinceCommand(const QString& filePath, const QDateTime& commandTime) const;
    
    // Get context as text based on valid commands
    QString generateContextText() const;

    // Observe a user query to manage topic switching / archive & restore.
    void observeUserQuery(const QString& text);

    // Graph context snapshot for UI visualization.
    QList<ContextGraphNodeInfo> getContextGraphNodes() const;
    QString activeContextGraphNodeId() const { return m_lastActiveGraphNodeId; }

    // Clear all context
    void clearAll();
    
    // Set the DevAssManager reference for file modification checks
    void setDevAssManager(DevAssManager* manager);
    
    // Set MainWindow reference to connect signals
    void setMainWindow(MainWindow* mainWindow);

    // Methods to manage command sequences
    QString startCommandSequence(const QString& name = QString(), const QString& description = QString());
    void endCommandSequence(const QString& sequenceId);
    void addCommandToSequence(Command* cmd, const QString& sequenceId, const QString& result = QString());
    QList<CommandSequenceInfo> getCommandSequences() const;
    QList<CommandContextInfo> getCommandsInSequence(const QString& sequenceId) const;
    CommandSequenceInfo getSequenceInfo(const QString& sequenceId) const;

    // Remove commands tied to a specific path (e.g. when the file was changed externally)
    void cleanupForPath(const QString& path, const QString& reason = QString());

    void clearActiveContextOnly();

signals:
    // Signal emitted when command context changes
    void contextChanged();
    void commandAdded(const QString& commandId);
    void commandInvalidated(const QString& commandId, const QString& reason);
    void commandRemoved(const QString& commandId);
    void sequenceStarted(const QString& sequenceId);
    void sequenceEnded(const QString& sequenceId);
    void commandAddedToSequence(const QString& commandId, const QString& sequenceId);

public slots:
    void onFileChanged(const QString &path);
    
private:
    QMap<QString, CommandContextInfo> m_commandHistory;
    QMap<QString, CommandSequenceInfo> m_sequenceHistory;
    QFileSystemWatcher* m_fileWatcher;
    DevAssManager* m_devAssManager;
    MainWindow* m_mainWindow;
    int m_commandCounter; // To generate unique IDs
    int m_sequenceCounter; // To generate unique sequence IDs

    QList<ArchivedContextChunk> m_archive;
    QStringList m_exhumedArchiveIds;
    QSet<QString> m_currentTopicKeywords;
    QString m_currentTopicTitle;
    QString m_archiveFilePath;

    QMap<QString, ContextGraphNodeInfo> m_contextGraphNodes;
    QMap<QString, QString> m_categoryRootNodeIds;
    QMap<QString, QString> m_activeGraphNodeByCategory;
    QString m_lastActiveGraphNodeId;
    QString m_contextGraphFilePath;
    int m_graphNodeCounter = 0;
    
    QString generateUniqueId(const QString& commandName);
    void rebuildFileWatcher();
    QStringList collectPathsFromParams(const QJsonObject& params) const;
    void addPathsToWatcher(const QStringList& paths);
    void cleanupEmptySequences();
    
    // Get file modification time
    QDateTime getFileModificationTime(const QString& path) const;

    QString generateContextTextInternal(bool includeArchived, int maxChars) const;
    QString formatCommandForContext(const CommandContextInfo& cmdInfo, int maxChars) const;
    QString formatSequenceForContext(const CommandSequenceInfo& seqInfo, int maxCharsPerCommand) const;

    void loadArchive();
    void saveArchive() const;
    void pruneArchive();
    void archiveAllActiveContext(const QString& reason, const QString& triggerQuery);
    void archiveOldestUntilFit(int maxActiveChars);
    void updateExhumedArchivesForQuery(const QString& queryText);
    QString graphContextText(int maxNodes, int maxActionsPerNode) const;
    void loadContextGraph();
    void saveContextGraph() const;
    void clearContextGraph();
    QString inferCategoryFromText(const QString& text) const;
    QString inferCategoryForCommand(const CommandContextInfo& info) const;
    QString ensureCategoryRootNode(const QString& category);
    QString createGraphNode(const QString& parentId,
                            const QString& category,
                            const QString& title,
                            const QString& summary,
                            const QStringList& tags,
                            bool milestone = false);
    void addActionToGraphNode(const QString& nodeId, const QString& action, bool milestone = false);
    void advanceContextGraphByUserQuery(const QString& text);
    void attachCommandToContextGraph(const CommandContextInfo& info);
    static QString normalizeCategory(const QString& category);
    static QString shortText(const QString& text, int maxChars);
    static QString defaultCategoryTitle(const QString& category);
    void pruneContextGraph();

    static QSet<QString> extractKeywords(const QString& text);
    static double jaccardSimilarity(const QSet<QString>& a, const QSet<QString>& b);
    static QString makeTopicTitle(const QString& text);
};

#endif // CONTEXTMANAGER_H
