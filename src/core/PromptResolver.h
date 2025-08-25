// PromptResolver.h
#pragma once
#include <QString>
#include <QStringList>
#include "PromptStep.h"

class MainWindow;   // forward

class PromptResolver
{
public:
    /* Подставляет все {{…}} в шаблон. Возвращает готовый запрос. */
    static QString resolve(const QString& tmpl,
                           const PromptStep& step,
                           MainWindow* ctx);
};
