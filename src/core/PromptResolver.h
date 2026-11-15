// PromptResolver.h
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include "PromptStep.h"

class MainWindow;   // forward

class PromptResolver
{
public:
    struct ResolvedPrompt
    {
        QString systemPrompt;
        QString userPrompt;
    };

    /* Fills the template placeholders and returns both the system prompt and user query. */
    static ResolvedPrompt resolve(const QString& tmpl,
                           const PromptStep& step,
                           MainWindow* ctx);

    /* A placeholder resolver available in templates.
     *  token: text inserted into the template, e.g. "PROJECT_ROOT".
     *  description: translated label shown in the insert list/combo. */
    struct ResolverInfo
    {
        QString token;
        QString description;
    };

    /* All available resolvers. */
    static QVector<ResolverInfo> availableResolvers();
    
    /* Set context for next sequence */
    static void setNextSequence(const QString& sequenceName, const QString& description);
    static QString getNextSequence();
    static void clearNextSequence();

private:
    static QString m_nextSequenceName;
    static QString m_nextSequenceDescription;
    
    /* Generate sequence info list for OPEN_SEQUENCE_INFO resolver */
    static QString generateSequenceInfoList(MainWindow* ctx);
};
