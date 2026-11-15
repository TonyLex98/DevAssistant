#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include "core/workflow/PromptStep.h"

class Session;

// Turns a step's template into the prompts sent to the model by filling its placeholders
// ({{USER_QUERY}}, {{OPEN_FILES}}, {{SUPPORTED_COMMANDS}}...) from the session.
class PromptResolver
{
public:
    struct ResolvedPrompt
    {
        QString systemPrompt;  // the filled template
        QString userPrompt;    // the user's request
    };

    static ResolvedPrompt resolve(const PromptStep& step, Session& session);

    // A placeholder the step editor offers: the token (e.g. "PROJECT_ROOT") and what it inserts.
    struct ResolverInfo
    {
        QString token;
        QString description;
    };
    static QVector<ResolverInfo> availableResolvers();

    // The documentation of the given commands, as the model is shown it.
    static QString commandDocs(const QStringList& commandNames);
};
