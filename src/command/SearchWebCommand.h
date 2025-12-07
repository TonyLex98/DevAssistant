#pragma once
#include "Command.h"
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

    // Переопределение метода для работы с LLM
    bool applyWithLLM(DevAssManager* devAss,
                      void* adapter,
                      QString* errorOut = nullptr) override;

private:
    QJsonObject m_params;
    QString m_description;
    QStringList m_results;
    QStringList m_rawResults;  // Store raw results for potential LLM summarization
    
    // Внутренняя функция для суммаризации контента
    QString summarizeContentInternal(const QString& content, const QString& query);
    
    // Новый метод для LLM-суммаризации
    QString generateLLMSummary(void* adapter, const QString& content, const QString& query);
};
