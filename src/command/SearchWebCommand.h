#pragma once
#include "Command.h"
#include <QStringList>

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

private:
    QJsonObject m_params;
    QString m_description;
    QStringList m_results;
    
    // Внутренняя функция для суммаризации контента
    QString summarizeContentInternal(const QString& content, const QString& query);
};
