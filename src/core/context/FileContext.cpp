#include "core/context/FileContext.h"
#include <algorithm>
#include <QFileInfo>

FileContext::FileContext()
{
}

void FileContext::setProjectRoot(const QString& root)
{
    m_projectRoot = root;
}

void FileContext::setPathCallbacks(
        std::function<QString(const QString&, const QString&)> normalize,
        std::function<QString(const QString&, const QString&)> display)
{
    m_normalizePath = normalize;
    m_displayPath = display;
}

void FileContext::setFullFileContext(const QString& path, const QStringList& lines)
{
    QString normalized = path;
    if (m_normalizePath)
        normalized = m_normalizePath(path, m_projectRoot);
    if (normalized.isEmpty())
        return;

    FileContextSlice& slice = m_contextSlices[normalized];
    slice.mode = FileContextSlice::FullFile;
    slice.fullFileLines = lines;
    slice.regions.clear();
    rebuildContextCache();
}

void FileContext::addRegionContext(const QString& path, int startLine, const QStringList& lines)
{
    QString normalized = path;
    if (m_normalizePath)
        normalized = m_normalizePath(path, m_projectRoot);
    if (normalized.isEmpty() || lines.isEmpty())
        return;

    FileContextSlice& slice = m_contextSlices[normalized];
    slice.mode = FileContextSlice::Ranges;
    slice.fullFileLines.clear();

    ContextRange incoming;
    incoming.startLine = startLine;
    incoming.lines = lines;
    incoming.endLine = startLine + lines.size() - 1;

    bool replaced = false;
    for (ContextRange& existing : slice.regions) {
        if (existing.startLine == incoming.startLine && existing.endLine == incoming.endLine) {
            existing.lines = lines;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        slice.regions.append(incoming);
        std::sort(slice.regions.begin(), slice.regions.end(),
                  [](const ContextRange& a, const ContextRange& b){
                      return a.startLine < b.startLine;
                  });
    }
    rebuildContextCache();
}

void FileContext::clearContextForPath(const QString& path)
{
    QString normalized = path;
    if (m_normalizePath)
        normalized = m_normalizePath(path, m_projectRoot);
    if (normalized.isEmpty())
        return;

    if (m_contextSlices.remove(normalized) > 0) {
        rebuildContextCache();
    }
}

void FileContext::clearAll()
{
    m_contextSlices.clear();
    m_consoleLogs.clear();
    m_externalSources.clear();
    rebuildContextCache();
}

void FileContext::clearContextForFiles(const QStringList& paths)
{
    for (const QString& path : paths) {
        clearContextForPath(path);
    }
}

void FileContext::appendConsoleLog(const QString& log)
{
    if (!log.isEmpty()) {
        m_consoleLogs << log;
        rebuildContextCache();
    }
}

void FileContext::appendConsoleContext(const QString& title, const QString& stdOut, const QString& stdErr, int exitCode)
{
    QString block = QString("==== %1 ====").arg(title);
    if (!stdOut.isEmpty())
        block += QString("\n[stdout]\n%1").arg(stdOut);
    if (!stdErr.isEmpty())
        block += QString("\n[stderr]\n%1").arg(stdErr);
    block += QString("\n[exit_code] %1").arg(exitCode);

    m_consoleLogs << block;
    rebuildContextCache();
}

void FileContext::clearConsoleLogs()
{
    m_consoleLogs.clear();
    rebuildContextCache();
}

void FileContext::removeConsoleLogsWithPrefix(const QString& prefix)
{
    auto it = std::remove_if(m_consoleLogs.begin(), m_consoleLogs.end(),
        [&prefix](const QString& log) {
            return log.startsWith(prefix);
        });
    m_consoleLogs.erase(it, m_consoleLogs.end());
    rebuildContextCache();
}

void FileContext::appendExternalSource(const QString& source)
{
    if (!source.isEmpty()) {
        m_externalSources << source;
        rebuildContextCache();
    }
}

void FileContext::clearExternalSources()
{
    m_externalSources.clear();
    rebuildContextCache();
}

const FileContext::FileContextSlice* FileContext::getSlice(const QString& path) const
{
    QString normalized = path;
    if (m_normalizePath)
        normalized = m_normalizePath(path, m_projectRoot);
    if (normalized.isEmpty())
        return nullptr;

    auto it = m_contextSlices.constFind(normalized);
    if (it != m_contextSlices.constEnd())
        return &it.value();
    return nullptr;
}

void FileContext::rebuildContextCache()
{
    QStringList blocks;
    for (auto it = m_contextSlices.cbegin(); it != m_contextSlices.cend(); ++it) {
        const FileContextSlice& slice = it.value();
        if (slice.mode == FileContextSlice::None)
            continue;

        QString block;
        if (m_displayPath) {
            block = QString("==== %1 ====")
                    .arg(m_displayPath(it.key(), m_projectRoot));
        } else {
            block = QString("==== %1 ====").arg(it.key());
        }
        if (slice.mode == FileContextSlice::Ranges)
            block += " (selected lines)";
        block += "\n";

        if (slice.mode == FileContextSlice::FullFile) {
            block += formatNumberedLines(slice.fullFileLines, 1);
        } else {
            for (const ContextRange& range : slice.regions) {
                block += formatNumberedLines(range.lines, range.startLine);
                block += "\n";
            }
            block = block.trimmed();
        }
        blocks << block.trimmed();
    }
    m_contextContent = blocks.join("\n\n");

    if (!m_consoleLogs.isEmpty()) {
        if (!m_contextContent.isEmpty())
            m_contextContent += "\n\n";
        m_contextContent += m_consoleLogs.join("\n\n");
    }
    if (!m_externalSources.isEmpty()) {
        if (!m_contextContent.isEmpty())
            m_contextContent += "\n\n";
        m_contextContent += m_externalSources.join("\n\n");
    }
}

QString FileContext::formatNumberedLines(const QStringList& lines, int startLine) const
{
    if (lines.isEmpty())
        return QString();
    const int width = QString::number(startLine + lines.size() - 1).size();
    QStringList numbered;
    for (int i = 0; i < lines.size(); ++i) {
        const int lineNo = startLine + i;
        numbered << QString("%1| %2")
                        .arg(lineNo, width, 10, QChar('0'))
                        .arg(lines.at(i));
    }
    return numbered.join("\n");
}
