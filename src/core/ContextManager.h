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

// Structure to track command context state
struct CommandContextInfo {
    QString id;
    QString name;
    QJsonObject params;
    QString description;
    QDateTime timestamp;
    bool isStillValid;           // Whether the command's result is still valid in context
    QString invalidationReason;  // Why the command was invalidated
    QString result;              // Command result/output
    
    CommandContextInfo() : isStillValid(true) {}
};

class ContextManager : public QObject
{
    Q_OBJECT

public:
    explicit ContextManager(QObject *parent = nullptr);
    ~ContextManager();

    // Add a command to context tracking
    void addCommand(Command* cmd, const QString& result = QString());
    
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

signals:
    // Signal emitted when command context changes
    void contextChanged();
    void commandAdded(const QString& commandId);
    void commandInvalidated(const QString& commandId, const QString& reason);
    void commandRemoved(const QString& commandId);

public slots:
    void onFileChanged(const QString &path);
    
private:
    QMap<QString, CommandContextInfo> m_commandHistory;
    QFileSystemWatcher* m_fileWatcher;
    DevAssManager* m_devAssManager;
    MainWindow* m_mainWindow;
    int m_commandCounter; // To generate unique IDs
    
    QString generateUniqueId(const QString& commandName);
    void setupFileWatcher();
    void rebuildFileWatcher();
    
    // Get file modification time
    QDateTime getFileModificationTime(const QString& path) const;
};

#endif // CONTEXTMANAGER_H