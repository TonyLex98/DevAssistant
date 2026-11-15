#pragma once
#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include "core/context/ContextManager.h"
#include "core/context/FileContext.h"

class Command;
class DevAssManager;

// Everything the model knows about the work so far, apart from the conversation itself:
// the commands it ran and their results (ContextManager), the files and lines it opened,
// console output and search results (FileContext). Opened files are watched: when one
// changes outside the program, what the context holds for it is dropped.
class WorkingContext : public QObject
{
    Q_OBJECT
public:
    explicit WorkingContext(QObject* parent = nullptr);

    // A new project: forgets the files of the previous one.
    void setProject(const QString& root, DevAssManager* manager);

    ContextManager& commands() { return m_commands; }
    const ContextManager& commands() const { return m_commands; }
    const FileContext& files() const { return m_files; }

    // Records a command and its result, inside the running sequence if there is one.
    void record(Command* command, const QString& result, const QString& sequenceId);
    // OPEN_FILE / OPEN_FILE_REGION: puts the file or the lines into the context.
    void open(Command* command, const QString& sequenceId);
    // After a command changed files: drops what the context holds for them.
    void forgetChangedFiles(Command* command);
    // Output of a program or of the model itself; the newest few blocks are kept.
    void addConsoleOutput(const QString& title, const QString& stdOut, const QString& stdErr, int exitCode);
    // Search results and other outside text; the newest few blocks are kept.
    void addExternal(const QString& block);
    // Images the user attached, as entries of the command history.
    void addImages(const QStringList& paths, const QStringList& summaries);
    // CONTEXT_CLEAR: the opened files and outputs; with `everything` the command history too.
    void clear(bool everything);
    // A new request from the user (the history decides what is still relevant).
    void observeRequest(const QString& text);

    // Texts for the prompts.
    QString openFilesText();                     // files, lines, console output, search results
    QString searchResultsText() const;
    QString commandChainText() const;            // every command with its state
    QString activeCommandsText() const;          // the commands still in context

    // The commands and sequences recorded since `since` (shown with an answer).
    struct Snapshot
    {
        QList<CommandContextInfo> commands;
        QList<CommandSequenceInfo> sequences;
    };
    Snapshot snapshotSince(const QDateTime& since) const;

signals:
    void changed();

private:
    void watch(const QString& path);
    void onFileChanged(const QString& path);
    QString normalize(const QString& path) const;

    static constexpr int kMaxBlocks = 5;

    QString m_root;
    ContextManager m_commands;
    FileContext m_files;
    QFileSystemWatcher m_watcher;
    QMap<QString, QSet<QString>> m_openedBy;  // file -> the commands that put it in context
};
