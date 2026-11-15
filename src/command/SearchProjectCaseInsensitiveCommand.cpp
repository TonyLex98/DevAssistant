#include "SearchProjectCaseInsensitiveCommand.h"
#include <DevAssManager.h>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegExp>
#include <QJsonArray>

SearchProjectCaseInsensitiveCommand::SearchProjectCaseInsensitiveCommand(const QString& query,
                                           int maxMatches,
                                           const QStringList& includeGlobs,
                                           const QString& descr)
    : m_includeGlobs(includeGlobs)
{
    m_params["query"] = query;
    m_params["maxMatches"] = qMax(1, maxMatches);
    m_params["caseSensitive"] = false; // Always case-insensitive
    if (!includeGlobs.isEmpty()) {
        QJsonArray arr;
        for (const QString& g : includeGlobs)
            arr.append(g);
        m_params["include"] = arr;
    }
    m_description = descr.isEmpty()
        ? QString("Case-insensitive project search: \"%1\"").arg(query)
        : descr;
}

bool SearchProjectCaseInsensitiveCommand::apply(DevAssManager* devAss, QString* err)
{
    m_matches.clear();
    if (!devAss) {
        if (err) *err = "SEARCH_PROJECT_CASE_INSENSITIVE: DevAssManager is not available";
        return false;
    }
    const QString query = m_params.value("query").toString();
    if (query.trimmed().isEmpty()) {
        if (err) *err = "SEARCH_PROJECT_CASE_INSENSITIVE: empty query";
        return false;
    }

    const int maxMatches = qMax(1, m_params.value("maxMatches").toInt(10));
    const bool caseSensitive = m_params.value("caseSensitive").toBool(false);
    const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QDir root = devAss->projectRoot();
    QString rootPath = root.absolutePath();

    QDirIterator it(rootPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && m_matches.size() < maxMatches) {
        const QString path = it.next();
        if (shouldSkipFile(path))
            continue;
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
            if (line.contains(query, cs)) {
                QString absPath = QFileInfo(path).absoluteFilePath();  // Use absolute path
                QString snippet = line.trimmed();
                if (snippet.size() > 200)
                    snippet = snippet.left(197) + "...";
                m_matches << QString("%1:%2\n%3").arg(absPath).arg(lineNo).arg(snippet);
            }
        }
    }

    if (m_matches.isEmpty() && err) {
        *err = "SEARCH_PROJECT_CASE_INSENSITIVE: no matches found";
    }
    return true;
}

bool SearchProjectCaseInsensitiveCommand::shouldSkipFile(const QString& path) const
{
    // Skip files in the project's DevelopersAssistant subfolder
    QString normalizedPath = QDir::cleanPath(path);
    QStringList pathParts = normalizedPath.split('/');
    for (const QString& part : pathParts) {
        if (part == "DevelopersAssistant") {
            return true;
        }
    }
    
    QFileInfo info(path);
    if (!m_includeGlobs.isEmpty()) {
        bool match = false;
        for (const QString& glob : m_includeGlobs) {
            QRegExp rx(glob, Qt::CaseInsensitive, QRegExp::Wildcard);
            if (rx.exactMatch(info.fileName())) {
                match = true;
                break;
            }
        }
        if (!match)
            return true;
    }
    static const QStringList binaryExt = {
        "png","jpg","jpeg","gif","bmp","ico","exe","dll","so","bin","pdf","zip","tar","gz"
    };
    if (binaryExt.contains(info.suffix(), Qt::CaseInsensitive))
        return true;
    return false;
}

QJsonObject SearchProjectCaseInsensitiveCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = m_description;
    return obj;
}

QString SearchProjectCaseInsensitiveCommand::getResult() const
{
    QString result = QString("SEARCH_PROJECT_CASE_INSENSITIVE: \"%1\"\n").arg(m_params.value("query").toString());
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