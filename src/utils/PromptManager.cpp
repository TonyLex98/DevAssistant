// PromptManager.cpp
#include "PromptManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "../command/CommandDocs.h"

PromptManager::PromptManager()
{
    // Default to the current directory
    m_projectRoot = QDir::current();
    m_devAssDir   = QDir(m_projectRoot.absoluteFilePath("DevelopersAssistant"));
    QSettings s;
    m_historySize = s.value("prompt/historySize", m_historySize).toInt();
    m_basePromptOverride = s.value("prompt/basePromptOverride", "").toString();
}

void PromptManager::setProjectRoot(const QString& path)
{
    m_projectRoot = QDir(path);
    m_devAssDir   = QDir(m_projectRoot.absoluteFilePath("DevelopersAssistant"));
}

/* ---------- Helper: file list ---------- */
QString PromptManager::fileListText() const
{
    QStringList lines;
    QDirIterator it(m_projectRoot.absolutePath(),
                    QDir::Files,
                    QDirIterator::Subdirectories);

    const QString devAssPath = m_devAssDir.absolutePath();

    while (it.hasNext()) {
        const QString file = it.next();

        /* skip files from the DevAss folder */
        if (file.startsWith(devAssPath))
            continue;

        QString rel = QDir(m_projectRoot.absolutePath()).relativeFilePath(file);
        lines.append(rel);
    }

    return lines.join("\n");
}


/* ---------- Full prompt ---------- */
QString PromptManager::buildPrompt(const QString& userText,
                                  const QString& capabilities, const QString&  buildProjectContext,
                                  const QList<QPair<QString,QString>>& history)
{
    QString p;
    Q_UNUSED(capabilities)

    if (!m_basePromptOverride.trimmed().isEmpty()) {
        p += m_basePromptOverride.trimmed();
        if (!p.endsWith('\n'))
            p += "\n";
        p += "\n";
    }

    /* 1. Assistant role */
    p += "You are an AI assistant that can only run the DSL commands listed below and nothing else!\n";
    p += "Your job is to help the user work on the project in the given directory and with the given files.\n";
    p += "If you lack information about the project when the user asks you something, first run the command that opens the needed files (OPEN_FILE) and nothing else! ";
    p += "Use OPEN_FILE separately from all other commands. After that you may use the others.\n";

    /* 2. Supported commands */
    p += "Supported commands:\n";
    p += supportedCommandsText();
    p += "Reply **only** with an array of JSON objects, one object per command, with no extra words. Always use absolute paths!\n\n";

    /* 3. All project files (without DevAss) */
    p += "Project root path and the list of all project files:\n";
    p += buildProjectContext;
    p += "\n\n";

    /* 3.1. Search command results */
    p += "Sources found so far (if empty, the search found NOTHING):\n{{SEARCH_RESULTS}}\n\n";

    /* 4. Dialog history */
    p += QString("Recent history (last %1 messages):\n").arg(m_historySize);
    int start = history.size() > m_historySize ? history.size() - m_historySize : 0;
    for (int i = start; i < history.size(); ++i)
        p += history[i].first + ": " + "<|message|>{{" + history[i].second + "}}" + "\n";

    /* 5. Current request */
    p += "\nThe current request that you, the AI assistant, **MUST** answer:\n" + userText;


    current_all_prompt = p;
    return p;
}

QDir PromptManager::projectRoot() const
{
    return m_projectRoot;
}

int PromptManager::tokenCount(const QString& text) const
{
    // 1 token ~ 4 characters, rounded up
    return text.size() / 4 + 1;
}

void PromptManager::setHistorySize(int value)
{
    m_historySize = qBound(0, value, 200);
    QSettings s;
    s.setValue("prompt/historySize", m_historySize);
}

void PromptManager::setBasePromptOverride(const QString& text)
{
    m_basePromptOverride = text;
    QSettings s;
    s.setValue("prompt/basePromptOverride", m_basePromptOverride);
}


QString PromptManager::supportedCommandsText() const
{
    QString result;

    for (Command* cmd : CommandRegistry::all()) {
        const CommandDoc doc = CommandDocs::doc(cmd->name());
        result += CommandDocs::renderForLLM(doc, true) + "\n\n";
        delete cmd;
        continue;

        QString name = cmd->name();
        QString description = cmd->description();

        /* Example JSON object, taken from the prototype command. */
        QJsonObject example = cmd->toJson();

        result += "- " + name + ": " + description + "\n";
        result += "  Format: " + QString::fromStdString(QJsonDocument(example).toJson(QJsonDocument::Compact).toStdString()) + "\n\n";

        delete cmd; // prototypes are read-only, delete them
    }

    return result;
}
