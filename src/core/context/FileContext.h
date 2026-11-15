#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <functional>

// The text the model is shown besides the prompt: whole files and line ranges it opened
// (with line numbers), console output of the programs it ran and search results.
// contextContent() is that text, rebuilt after every change.
class FileContext
{
public:
    struct ContextRange {
        int startLine = -1;
        int endLine = -1;
        QStringList lines;
    };

    struct FileContextSlice {
        enum Mode { None, FullFile, Ranges } mode = None;
        QStringList fullFileLines;
        QVector<ContextRange> regions;
    };

    FileContext();

    // Get the generated context text
    const QString& contextContent() const { return m_contextContent; }

    // Set/clear context for a file
    void setFullFileContext(const QString& path, const QStringList& lines);
    void addRegionContext(const QString& path, int startLine, const QStringList& lines);
    void clearContextForPath(const QString& path);

    // Clear all context
    void clearAll();

    // Console logs
    void appendConsoleLog(const QString& log);
    void appendConsoleContext(const QString& title, const QString& stdOut, const QString& stdErr, int exitCode);
    void clearConsoleLogs();
    void removeConsoleLogsWithPrefix(const QString& prefix);
    QStringList& consoleLogsMutable() { return m_consoleLogs; }
    const QStringList& consoleLogs() const { return m_consoleLogs; }

    // External sources
    void appendExternalSource(const QString& source);
    void clearExternalSources();
    const QStringList& externalSources() const { return m_externalSources; }

    // Clear context for files that match a pattern (for cleanup after batch edits)
    void clearContextForFiles(const QStringList& paths);

    // Get slice for a path (for inspection)
    const FileContextSlice* getSlice(const QString& path) const;

    // Number of tracked files
    int fileCount() const { return m_contextSlices.size(); }

    // Lines with their numbers, as the model sees them.
    QString formatNumberedLines(const QStringList& lines, int startLine) const;

    // Rebuilds contextContent().
    void rebuildContextCache();

private:

    QMap<QString, FileContextSlice> m_contextSlices;
    QString m_contextContent;
    QStringList m_consoleLogs;
    QStringList m_externalSources;

    // How paths are normalized and shown (set by WorkingContext).
    std::function<QString(const QString&, const QString&)> m_normalizePath;
    std::function<QString(const QString&, const QString&)> m_displayPath;
    QString m_projectRoot;

public:
    void setProjectRoot(const QString& root);
    void setPathCallbacks(std::function<QString(const QString&, const QString&)> normalize,
                          std::function<QString(const QString&, const QString&)> display);
};
