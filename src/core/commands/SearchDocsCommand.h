#pragma once
#include "core/commands/Command.h"

// SEARCH_DOCS: semantic search in the project's document index (Settings > Documents).
// Returns the closest excerpts with their files, for the model to read and cite.
class SearchDocsCommand : public Command
{
public:
    explicit SearchDocsCommand(const QString& query, int k = 5, const QString& descr = QString());

    QString name() const override { return "SEARCH_DOCS"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;
    QString getResult() const override { return m_result; }

private:
    QJsonObject m_params;
    QString m_description;
    QString m_result;
};
