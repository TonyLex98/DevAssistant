#include "SearchProjectCommand.h"
#include <DevAssManager.h>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegExp>
#include <QJsonArray>

SearchProjectCommand::SearchProjectCommand(const QString& query,
                                           int maxMatches,
                                           bool caseSensitive,
                                           const QStringList& includeGlobs,
                                           const QString& descr)
    : m_includeGlobs(includeGlobs)
{
    m_params["query"] = query;
    m_params["maxMatches"] = qMax(1, maxMatches);
    m_params["caseSensitive"] = caseSensitive;
    if (!includeGlobs.isEmpty()) {
        QJsonArray arr;
        for (const QString& g : includeGlobs)
            arr.append(g);
        m_params["include"] = arr;
    }
    m_description = descr.isEmpty()
        ? QString("Поиск по проекту: \"%1\"").arg(query)
        : descr;
}

bool SearchProjectCommand::apply(DevAssManager* devAss, QString* err)
{
    m_matches.clear();
    if (!devAss) {
        if (err) *err = "SEARCH_PROJECT: DevAssManager недоступен";
        return false;
    }
    const QString query = m_params.value("query").toString();
    if (query.trimmed().isEmpty()) {
        if (err) *err = "SEARCH_PROJECT: пустой запрос";
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
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QTextStream in(&f);
        int lineNo = 0;
        while (!in.atEnd() && m_matches.size() < maxMatches) {
            const QString line = in.readLine();
            ++lineNo;
            if (line.contains(query, cs)) {
                QString rel = root.relativeFilePath(path);
                QString snippet = line.trimmed();
                if (snippet.size() > 200)
                    snippet = snippet.left(197) + "...";
                m_matches << QString("%1:%2\n%3").arg(rel).arg(lineNo).arg(snippet);
            }
        }
        f.close();
    }

    if (m_matches.isEmpty() && err) {
        *err = "SEARCH_PROJECT: совпадения не найдены";
    }
    return true;
}

bool SearchProjectCommand::shouldSkipFile(const QString& path) const
{
    if (path.contains("DevelopersAssistant"))
        return true;
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

QJsonObject SearchProjectCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = m_description;
    return obj;
}
