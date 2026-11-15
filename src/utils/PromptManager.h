// PromptManager.h
#pragma once
#include <QString>
#include <QStringList>
#include <QDir>
#include <QDebug>
#include <QSettings>
#include <CommandRegistry.h>

class PromptManager
{
public:
    PromptManager();

    /* -------------  Project path ------------- */
    void setProjectRoot(const QString& path);

    /* -------------  Supported commands ------------- */
    QString supportedCommandsText() const;
    QString capabilitiesText() const;

    /* -------------  Main prompt ------------- */
    QString buildPrompt(const QString& userText,
                        const QString& capabilities, const QString&  buildProjectContext,
                        const QList<QPair<QString,QString>>& history);

    /* -------------  Paths ------------- */
    QDir projectRoot() const;
    QDir devAssDir()   const { return m_devAssDir; }

    /* -------------  File list helper ------------- */
    QString fileListText() const;

    int historySize(){
        return m_historySize;
    }
    void setHistorySize(int value);
    void setBasePromptOverride(const QString& text);
    QString basePromptOverride() const { return m_basePromptOverride; }

    /** @brief Estimates the token count of a string.
     *
     *  Formula: ceil(text.length() / 4)
     *  @param text Text to count
     *  @return Approximate number of tokens
     */
    int tokenCount(const QString& text) const;

    QString current_all_prompt;

private:


    /* -------------  Paths ------------- */
    QDir m_projectRoot;   // project root
    QDir m_devAssDir;     // <project>/DevAss
    int m_historySize = 20;
    QString m_basePromptOverride;
};
