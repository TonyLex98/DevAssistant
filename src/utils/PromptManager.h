#pragma once
#include <QString>
#include <QStringList>

class PromptManager {
public:
    PromptManager();

    QString basePromptPath() const;
    QString loadBasePrompt() const;
    void saveBasePrompt(const QString& text) const;

    int historySize() const { return m_historySize; }
    void setHistorySize(int n) { m_historySize = n; }

    QString buildPrompt(const QString& userText,
                        const QString& projectContext,
                        const QString& capabilities,
                        const QList<QPair<QString,QString>>& history) const;

private:
    int m_historySize = 6;
};
