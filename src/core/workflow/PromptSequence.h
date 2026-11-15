// PromptSequence.h
#pragma once
#include <QString>
#include <QVector>
#include "core/workflow/PromptStep.h"

class PromptSequence
{
public:
    PromptSequence() = default;
    explicit PromptSequence(const QString& name) : m_name(name) {}

    /*--- serialization ---------------------------------------------------*/
    QJsonObject toJson() const;
    static PromptSequence fromJson(const QJsonObject& obj);

    /*--- basic operations ------------------------------------------------*/
    void addStep(const PromptStep& step) { m_steps.append(step); }
    void insertStep(int idx, const PromptStep& step) { m_steps.insert(idx, step); }
    void removeStep(int idx) { m_steps.removeAt(idx); }
    void moveStep(int from, int to) { m_steps.move(from, to); }
    void setStep(int idx, const PromptStep& step) {
        if (idx < 0 || idx >= m_steps.size())
            return;
        m_steps[idx] = step;
    }

    /*--- getters ---------------------------------------------------------*/
    const QString&               name() const { return m_name; }
    const QString&               description() const { return m_description; }
    const QVector<PromptStep>&   steps() const { return m_steps; }

    void setName(const QString& n) { m_name = n; }
    void setDescription(const QString& d) { m_description = d; }

private:
    QString          m_name;
    QString          m_description;
    QVector<PromptStep> m_steps;
};
