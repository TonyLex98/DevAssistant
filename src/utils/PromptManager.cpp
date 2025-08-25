// PromptManager.cpp
#include "PromptManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

PromptManager::PromptManager()
{
    // по‑умолчанию можно задать текущий каталог
    m_projectRoot = QDir::current();
    m_devAssDir   = QDir(m_projectRoot.absoluteFilePath("DevelopersAssistant"));
}

void PromptManager::setProjectRoot(const QString& path)
{
    m_projectRoot = QDir(path);
    m_devAssDir   = QDir(m_projectRoot.absoluteFilePath("DevelopersAssistant"));
}

/* ---------- Вспомогательная функция – список файлов ---------- */
QString PromptManager::fileListText() const
{
    QStringList lines;
    QDirIterator it(m_projectRoot.absolutePath(),
                    QDir::Files,
                    QDirIterator::Subdirectories);

    const QString devAssPath = m_devAssDir.absolutePath();

    while (it.hasNext()) {
        const QString file = it.next();

        /* пропускаем файлы из каталога DevAss */
        if (file.startsWith(devAssPath))
            continue;

        QString rel = QDir(m_projectRoot.absolutePath()).relativeFilePath(file);
        lines.append(rel);
    }

    return lines.join("\n");
}


/* ---------- Полный промпт ---------- */
QString PromptManager::buildPrompt(const QString& userText,
                                  const QString& capabilities, const QString&  buildProjectContext,
                                  const QList<QPair<QString,QString>>& history)
{
    QString p;

    /* 1. Роль ассистента */
    p += "Ты – AI‑ассистент, который может выполнять только команды DSL, перечисленные ниже и ничего больше!!!!\n";
    p += "Твоя задача помогать работать пользователю над проектом в указанной дерриктории и с указанными файлами.\n";
    p += "Если тебе недостаточно данных о проекте когда пользователь тебя спрашивает сначала обязательно примени комманду открыть нужные файлы (OPEN_FILE) и никаких больше!!!";
    p += "Используй OPEN_FILE отдельно от всех других комманд. После этого можешь использовать другие\n";

    /* 2. Поддерживаемые команды*/
    p += "Поддерживаемые команды:\n";
    p += supportedCommandsText();
    p += "Ответьте **только** массивом JSON‑объектов, по одному объекту на команду, без лишних слов. Пути всегда указывай абсолютные!\n\n";

    /* 3. Список всех файлов проекта (без DevAss) */
    p += "Корневой путь и список всех файлов проекта:\n";
    p += buildProjectContext;
    p += "\n\n";

    /* 4. История диалога */
    p += QString("Последняя история (последние %1 сообщений):\n").arg(m_historySize);
    int start = history.size() > m_historySize ? history.size() - m_historySize : 0;
    for (int i = start; i < history.size(); ++i)
        p += history[i].first + ": " + "<|message|>{{" + history[i].second + "}}" + "\n";

    /* 5. Текущий запрос */
    p += "\nТекущий запрос, на который ты, AI‑ассистент **ОБЯЗАН** ответить:\n" + userText;


    current_all_prompt = p;
    return p;
}

int PromptManager::tokenCount(const QString& text) const
{
    // 1 токен ≈ 4 символа → округляем вверх
    return text.size() / 4 + 1;
}


// PromptManager.cpp (или отдельный helper)
QString PromptManager::supportedCommandsText() const
{
    QString result;

    for (Command* cmd : CommandRegistry::all()) {
        QString name = cmd->name();

        /* Получаем «пример» JSON‑объекта.  Можно взять сразу fromJson
           или вручную сконструировать – здесь берём просто команду‑шаблон. */
        QJsonObject example = cmd->toJson();

        result += "- " + name + "\n";
        result += "  Формат: " + QString::fromStdString(QJsonDocument(example).toJson(QJsonDocument::Compact).toStdString()) + "\n\n";

        delete cmd; // шаблоны только для чтения – удаляем
    }

    return result;
}

