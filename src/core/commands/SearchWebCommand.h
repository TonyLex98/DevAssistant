#pragma once
#include "core/commands/Command.h"
#include <QStringList>
#include <QString>


class SearchWebCommand : public Command
{
public:
    SearchWebCommand(const QString& query = QString(),
                     int maxResults = 5,
                     const QString& descr = QString());

    QString name() const override { return "SEARCH_WEB"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;

    const QStringList& results() const { return m_results; }
    const QStringList& rawResults() const { return m_rawResults; }
    QString query() const { return m_params.value("query").toString(); }

    // Waits on the network: run in a worker thread.
    bool runsInBackground() const override { return true; }
    // The text found on the pages (or the snippets), for a summary by the model; empty if none.
    QString summarySource() const { return m_summarySource; }

private:
    QJsonObject m_params;
    QString m_description;
    QStringList m_results;
    QStringList m_rawResults;  // the links found, one block per hit
    QString m_summarySource;

    // Extractive summary without the LLM.
    QString summarizeContentInternal(const QString& content, const QString& query);

};
