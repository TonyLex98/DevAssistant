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

    /* Структура описания доступного резолвера.
     *  token – текст, который будет вставлен в шаблон, например "PROJECT_ROOT".
     *  description – русское описание, которое будет выводиться в подсказке к кнопке. */
    struct ResolverInfo
    {
        QString token;
        QString description;
    };

    /* Список всех доступных резолверов. */
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
