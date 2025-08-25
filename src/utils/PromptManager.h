// PromptManager.h
#pragma once
#include <QString>
#include <QStringList>
#include <QDir>
#include <QDebug>
#include <CommandRegistry.h>

class PromptManager
{
public:
    PromptManager();

    /* -------------  Настройка пути проекта ------------- */
    void setProjectRoot(const QString& path);      // <-- новое

    /* -------------  Поддерживаемые команды ------------- */
    QString supportedCommandsText() const;         // ← существующая
    QString capabilitiesText() const;             // ← существующая

    /* -------------  Основной промпт ------------- */
    QString buildPrompt(const QString& userText,
                        const QString& capabilities, const QString&  buildProjectContext,
                        const QList<QPair<QString,QString>>& history);   // <-- изменено

    /* -------------  Хранение путей ------------- */
    QDir projectRoot() const { return m_projectRoot; }
    QDir devAssDir()   const { return m_devAssDir; }

    /* -------------  Вспомогательный список файлов ------------- */
    QString fileListText() const;  // <-- новое

    int historySize(){
        return m_historySize;
    }

    /** @brief Оценка количества токенов строки.
     *
     *  Формула: ceil(text.length() / 4)
     *  @param text Текст, который нужно посчитать
     *  @return Приблизительное число токенов
     */
    int tokenCount(const QString& text) const;

    QString current_all_prompt;

private:


    /* -------------  Храним пути ------------- */
    QDir m_projectRoot;   // корень проекта
    QDir m_devAssDir;     // путь к <project>/DevAss
    int m_historySize = 20;
};
