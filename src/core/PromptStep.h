// PromptStep.h
#pragma once
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

class PromptStep
{
public:
    PromptStep() = default;
    explicit PromptStep(const QString& tmpl,
                        const QStringList& allowed = {})
        : m_template(tmpl), m_allowedCommands(allowed) {}

    /*--- сериализация ---------------------------------------------------*/
    QJsonObject toJson() const;
    static PromptStep fromJson(const QJsonObject& obj);

    /*--- геттеры/сеттеры ------------------------------------------------*/
    const QString&   tmpl() const { return m_template; }
    void setTemplate(const QString& t) { m_template = t; }

    const QStringList& allowedCommands() const { return m_allowedCommands; }
    void setAllowedCommands(const QStringList& lst) { m_allowedCommands = lst; }

private:
    QString      m_template;          // шаблон промпта
    QStringList  m_allowedCommands;   // имена команд из CommandRegistry
};
