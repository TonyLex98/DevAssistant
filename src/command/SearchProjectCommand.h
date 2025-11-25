#pragma once
#include "Command.h"
#include <QStringList>

class SearchProjectCommand : public Command
{
public:
    SearchProjectCommand(const QString& query = QString(),
                         int maxMatches = 10,
                         bool caseSensitive = false,
                         const QStringList& includeGlobs = QStringList(),
                         bool useRegex = false,
                         int contextLines = 0,
                         const QStringList& includePaths = QStringList(),
                         const QStringList& excludePaths = QStringList(),
                         const QString& descr = QString());

    QString name() const override { return "SEARCH_PROJECT"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;

    const QStringList& matches() const { return m_matches; }

    QString getResult() const override;

private:
    bool shouldSkipFile(const QString& path) const;

    QJsonObject m_params;
    QString m_description;
    QStringList m_includeGlobs;
    QStringList m_includePaths;
    QStringList m_excludePaths;
    QStringList m_matches;
};
