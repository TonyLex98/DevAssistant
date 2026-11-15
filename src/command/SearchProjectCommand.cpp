#include "SearchProjectCommand.h"
#include <DevAssManager.h>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegExp>
#include <QJsonArray>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

SearchProjectCommand::SearchProjectCommand(const QString& query,
                                           int maxMatches,
                                           bool caseSensitive,
                                           const QStringList& includeGlobs,
                                           bool useRegex,
                                           int contextLines,
                                           const QStringList& includePaths,
                                           const QStringList& excludePaths,
                                           const QString& descr)
    : m_includeGlobs(includeGlobs)
    , m_includePaths(includePaths)
    , m_excludePaths(excludePaths)
{
    m_params["query"] = query;
    m_params["maxMatches"] = qMax(1, maxMatches);
    m_params["caseSensitive"] = caseSensitive;
    m_params["useRegex"] = useRegex;
    m_params["contextLines"] = contextLines;
    if (!includeGlobs.isEmpty()) {
        QJsonArray arr;
        for (const QString& g : includeGlobs)
            arr.append(g);
        m_params["include"] = arr;
    }
    if (!includePaths.isEmpty()) {
        QJsonArray arr;
        for (const QString& path : includePaths)
            arr.append(path);
        m_params["includePaths"] = arr;
    }
    if (!excludePaths.isEmpty()) {
        QJsonArray arr;
        for (const QString& path : excludePaths)
            arr.append(path);
        m_params["excludePaths"] = arr;
    }
    m_description = descr.isEmpty()
        ? QString("Searches for text in all project files. "
                  "Example: { \"command\": \"SEARCH_PROJECT\", \"params\": { \"query\": \"class MainWindow\", \"maxMatches\": 5 } }")
        : descr;
}

bool SearchProjectCommand::apply(DevAssManager* devAss, QString* err)
{
    m_matches.clear();
    if (!devAss) {
        if (err) *err = "SEARCH_PROJECT: DevAssManager is not available";
        return false;
    }
    const QString query = m_params.value("query").toString();
    if (query.trimmed().isEmpty()) {
        if (err) *err = "SEARCH_PROJECT: empty query";
        return false;
    }

    const int maxMatches = qMax(1, m_params.value("maxMatches").toInt(10));
    const bool caseSensitive = m_params.value("caseSensitive").toBool(false);
    const bool useRegex = m_params.value("useRegex").toBool(false);
    const int contextLines = m_params.value("contextLines").toInt(0);
    const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QDir root = devAss->projectRoot();
    QString rootPath = root.absolutePath();

    // includePaths / excludePaths are usually relative ("src", "build"); the files they are
    // compared with are absolute, so resolve them against the project first.
    for (QStringList* list : {&m_includePaths, &m_excludePaths}) {
        for (QString& p : *list) {
            if (!QFileInfo(p).isAbsolute())
                p = QDir::cleanPath(QDir(rootPath).absoluteFilePath(p));
        }
    }

    // Collect the files to search first (pre-filtering)
    QStringList filePaths;
    QDirIterator it(rootPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && filePaths.size() < 10000) { // cap the file list to bound memory use
        const QString path = it.next();
        if (!shouldSkipFile(path)) {
            filePaths << path;
        }
    }

    // Build the regex if needed
    QRegularExpression regex;
    if (useRegex) {
        QRegularExpression::PatternOptions options = caseSensitive ?
            QRegularExpression::NoPatternOption :
            QRegularExpression::CaseInsensitiveOption;
        regex = QRegularExpression(query, options);
    }

    for (const QString& path : filePaths) {
        if (m_matches.size() >= maxMatches) break;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;

        QByteArray content = f.readAll();
        f.close();

        // Try to decode with UTF-8 first, fallback to system encoding if needed
        QString text = QString::fromUtf8(content);
        if (text.isNull() || (content.size() > 0 && text.toUtf8() != content)) {
            // Fallback to system locale encoding if UTF-8 doesn't work
            text = QString::fromLocal8Bit(content);
        }

        QStringList lines = text.split('\n', Qt::KeepEmptyParts);
        for (int i = 0; i < lines.size() && m_matches.size() < maxMatches; ++i) {
            const QString& line = lines[i];
            int lineNo = i + 1; // Line numbers start from 1

            bool found = false;
            if (useRegex) {
                QRegularExpressionMatch match = regex.match(line);
                found = match.hasMatch();
            } else {
                found = line.contains(query, cs);
            }

            if (found) {
                QString absPath = QFileInfo(path).absoluteFilePath();  // Use absolute path
                QString matchLine = line.trimmed();

                QString result;
                if (contextLines > 0) {
                    // Context lines before and after the match
                    int start = qMax(0, i - contextLines);
                    int end = qMin(lines.size(), i + contextLines + 1);

                    QStringList contextLinesList;
                    for (int j = start; j < end; j++) {
                        QString prefix = (j == i) ? QString(">>>%1>>> ").arg(j + 1) : QString("%1 ").arg(j + 1);
                        QString contextLine = lines[j].trimmed();
                        if (contextLine.size() > 200)
                            contextLine = contextLine.left(197) + "...";
                        contextLinesList << prefix + contextLine;
                    }
                    result = QString("%1:%2\n%3").arg(absPath).arg(lineNo).arg(contextLinesList.join("\n"));
                } else {
                    // The matching line only
                    if (matchLine.size() > 200)
                        matchLine = matchLine.left(197) + "...";
                    result = QString("%1:%2\n%3").arg(absPath).arg(lineNo).arg(matchLine);
                }

                m_matches << result;
            }
        }
    }

    if (m_matches.isEmpty() && err) {
        *err = "SEARCH_PROJECT: no matches found";
    }
    return true;
}

bool SearchProjectCommand::shouldSkipFile(const QString& path) const
{
    QString normalizedPath = QDir::cleanPath(path);
    QFileInfo fileInfo(path);
    QString fileName = fileInfo.fileName();
    QString filePath = fileInfo.absolutePath();

    // Excluded directories
    if (!m_excludePaths.isEmpty()) {
        for (const QString& excludePath : m_excludePaths) {
            QFileInfo excludeInfo(excludePath);
            QString absoluteExcludePath = excludeInfo.isAbsolute() ?
                excludeInfo.absoluteFilePath() : QDir::cleanPath(excludePath);

            if (normalizedPath.startsWith(absoluteExcludePath + "/") ||
                normalizedPath.startsWith(absoluteExcludePath + "\\") ||
                normalizedPath == absoluteExcludePath) {
                return true;
            }
        }
    }

    // If include directories are set, the file must be inside one of them
    if (!m_includePaths.isEmpty()) {
        bool inIncludePath = false;
        for (const QString& includePath : m_includePaths) {
            QFileInfo includeInfo(includePath);
            QString absoluteIncludePath = includeInfo.isAbsolute() ?
                includeInfo.absoluteFilePath() : QDir::cleanPath(includePath);

            if (filePath.startsWith(absoluteIncludePath + "/") ||
                filePath.startsWith(absoluteIncludePath + "\\") ||
                filePath == absoluteIncludePath) {
                inIncludePath = true;
                break;
            }
        }
        if (!inIncludePath) {
            return true;
        }
    }

    // Skip files in the project's DevelopersAssistant subfolder
    QStringList pathParts = normalizedPath.split('/');
    for (const QString& part : pathParts) {
        if (part == "DevelopersAssistant") {
            return true;
        }
    }

    // File masks
    if (!m_includeGlobs.isEmpty()) {
        bool match = false;
        for (const QString& glob : m_includeGlobs) {
            QRegExp rx(glob, Qt::CaseInsensitive, QRegExp::Wildcard);
            if (rx.exactMatch(fileName)) {
                match = true;
                break;
            }
        }
        if (!match)
            return true;
    }

    // Binary extensions
    static const QStringList binaryExt = {
        "png","jpg","jpeg","gif","bmp","ico","exe","dll","so","bin","pdf","zip","tar","gz"
    };
    if (binaryExt.contains(fileInfo.suffix(), Qt::CaseInsensitive))
        return true;

    return false;
}

QJsonObject SearchProjectCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = m_description;
    return obj;
}

QString SearchProjectCommand::getResult() const
{
    QString result = QString("SEARCH_PROJECT: \"%1\"\n").arg(m_params.value("query").toString());
    if (!m_matches.isEmpty()) {
        result += QString("[matches]\n%1\n").arg(m_matches.join("\n\n"));
        result += "exitCode=0";
    } else {
        result += "[matches]\nNo matches found\n";
        result += "exitCode=1";
    }
    
    // Add error message if command failed
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}
