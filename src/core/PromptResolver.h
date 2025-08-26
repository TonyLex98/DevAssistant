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
};
