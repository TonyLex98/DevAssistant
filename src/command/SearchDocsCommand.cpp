#include "SearchDocsCommand.h"

#include "../../ragmodule/RagIndex.h"

#include <QDir>

SearchDocsCommand::SearchDocsCommand(const QString& query, int k, const QString& descr)
{
    m_params["query"] = query;
    m_params["k"] = k;
    m_description = descr.isEmpty()
        ? QStringLiteral("Semantic search in the indexed project documents. "
                         "Example: { \"command\": \"SEARCH_DOCS\", \"params\": { \"query\": \"how is the config loaded\", \"k\": 5 } }")
        : descr;
}

bool SearchDocsCommand::apply(DevAssManager* devAss, QString* err)
{
    const QString query = m_params.value("query").toString().trimmed();
    if (query.isEmpty()) {
        if (err) *err = "SEARCH_DOCS: query is empty";
        return false;
    }
    if (!devAss) {
        if (err) *err = "SEARCH_DOCS: no project is open";
        return false;
    }
    const QString root = devAss->projectRoot().absolutePath();
    const int k = qBound(1, m_params.value("k").toInt(5), 20);

    QString error;
    const QVector<Rag::Hit> hits = Rag::Index::search(root, query, k, &error);
    if (hits.isEmpty()) {
        if (err) *err = QStringLiteral("SEARCH_DOCS: %1").arg(error.isEmpty() ? QStringLiteral("nothing found") : error);
        return false;
    }

    QStringList parts;
    for (int i = 0; i < hits.size(); ++i) {
        const Rag::Hit& h = hits.at(i);
        QString text = h.chunk.text;
        if (text.size() > 1500)
            text = text.left(1500) + QStringLiteral("\n[...]");
        parts << QStringLiteral("[%1] %2 (part %3, score %4)\n%5")
                 .arg(i + 1)
                 .arg(QDir(root).relativeFilePath(h.chunk.path))
                 .arg(h.chunk.ordinal + 1)
                 .arg(QString::number(h.score, 'f', 2), text);
    }
    m_result = QStringLiteral("SEARCH_DOCS \"%1\"\n\n%2").arg(query, parts.join(QStringLiteral("\n\n")));
    return true;
}

QJsonObject SearchDocsCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}
