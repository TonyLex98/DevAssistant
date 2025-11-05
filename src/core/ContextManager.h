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
    
    QString generateUniqueId(const QString& commandName);
    void setupFileWatcher();
    void rebuildFileWatcher();
    
    // Get file modification time
    QDateTime getFileModificationTime(const QString& path) const;
};

#endif // CONTEXTMANAGER_H