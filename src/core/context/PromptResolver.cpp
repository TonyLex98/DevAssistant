#include "core/context/PromptResolver.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <functional>
#include "core/commands/Command.h"
#include "core/commands/CommandDocs.h"
#include "core/commands/CommandRegistry.h"
#include "core/session/Session.h"
#include "core/workflow/WorkflowLibrary.h"

namespace {

QString truncateForPrompt(const QString& text, int maxLines, int maxChars)
{
    QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    if (maxLines > 0 && lines.size() > maxLines) {
        const int head = qMax(1, maxLines - 50);
        const int tail = qMin(50, maxLines / 4);
        lines = lines.mid(0, head) + QStringList{QStringLiteral("...")} + lines.mid(lines.size() - tail);
    }
    QString out = lines.join(QLatin1Char('\n'));
    if (maxChars > 0 && out.size() > maxChars)
        out = out.left(maxChars) + QStringLiteral("\n...");
    return out;
}

QString utcOffset(int seconds)
{
    const int abs = qAbs(seconds);
    return QStringLiteral("UTC%1%2:%3")
            .arg(seconds >= 0 ? QLatin1Char('+') : QLatin1Char('-'))
            .arg(abs / 3600, 2, 10, QLatin1Char('0'))
            .arg((abs % 3600) / 60, 2, 10, QLatin1Char('0'));
}

QString sequenceLine(int index, const QString& description, const QString& fileBaseName, bool summarize)
{
    return QStringLiteral("%1. %2; sequenceName = \"%3\"; summarize = %4;")
            .arg(index)
            .arg(description)
            .arg(fileBaseName, summarize ? QStringLiteral("true") : QStringLiteral("false"));
}

// {{OPEN_SEQUENCE_INFO}}: where the running sequence may go next. A template lists the links
// of each sequence ("sequenceTargets"); without them, every sequence in its folder.
QString sequenceInfo(Session& session)
{
    const QString currentPath = session.run().sequencePath;
    QStringList lines;
    int index = 1;

    if (currentPath.isEmpty()) {
        const QMap<QString, PromptSequence>& all = session.workflows().sequences().sequences();
        for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
            const QString description = it.value().description().isEmpty() ? it.value().name() : it.value().description();
            lines << sequenceLine(index++, description, QFileInfo(it.key()).baseName(), false);
        }
        return lines.isEmpty() ? QStringLiteral("No sequences available") : lines.join(QLatin1Char('\n'));
    }

    const QFileInfo current(currentPath);
    const QDir dir = current.absoluteDir();
    QFile templateFile(dir.filePath(QStringLiteral("template.templ.json")));
    if (templateFile.open(QIODevice::ReadOnly)) {
        const QJsonObject targets = QJsonDocument::fromJson(templateFile.readAll()).object()
                                            .value(QStringLiteral("sequenceTargets")).toObject();
        for (const QJsonValue& target : targets.value(current.baseName()).toArray()) {
            const QString baseName = target.toString();
            const WorkflowLibrary::Found f = WorkflowLibrary::load(dir.filePath(baseName + QStringLiteral(".seq.json")));
            QString description = f.isValid() ? f.sequence.description() : QString();
            if (description.isEmpty())
                description = f.isValid() ? f.sequence.name() : baseName;
            lines << sequenceLine(index++, description, baseName, true);
        }
    }
    if (lines.isEmpty()) {
        const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.seq.json")}, QDir::Files | QDir::NoSymLinks, QDir::Name);
        for (const QFileInfo& file : files) {
            const WorkflowLibrary::Found f = WorkflowLibrary::load(file.absoluteFilePath());
            if (!f.isValid())
                continue;
            const QString description = f.sequence.description().isEmpty() ? f.sequence.name() : f.sequence.description();
            lines << sequenceLine(index++, description, file.baseName(), false);
        }
    }
    return lines.isEmpty() ? QStringLiteral("No sequences available") : lines.join(QLatin1Char('\n'));
}

QString previousStepsText(const RunState& run)
{
    if (run.completedSteps.isEmpty())
        return QStringLiteral("Nothing has been done yet.");
    QString out;
    for (const QString& s : run.completedSteps)
        out += QStringLiteral(" - ") + s + QLatin1Char('\n');
    return out.trimmed();
}

} // namespace

QString PromptResolver::commandDocs(const QStringList& commandNames)
{
    QStringList out;
    for (Command* c : CommandRegistry::all()) {
        if (commandNames.contains(c->name()))
            out << CommandDocs::renderForLLM(CommandDocs::doc(c->name()), true);  // compact for small models
        delete c;
    }
    return out.join(QStringLiteral("\n\n"));
}

PromptResolver::ResolvedPrompt PromptResolver::resolve(const PromptStep& step, Session& session)
{
    const Conversation& conversation = session.conversation();
    const Project& project = session.project();
    WorkingContext& context = session.context();
    const QString userQuery = conversation.currentRequest();

    const QDateTime now = QDateTime::currentDateTime();
    const QString date = now.date().toString(QStringLiteral("yyyy-MM-dd"));
    const QString dateTime = QStringLiteral("%1 %2 (%3)")
            .arg(date, now.time().toString(QStringLiteral("HH:mm:ss")), utcOffset(now.offsetFromUtc()));

    // A placeholder is computed only if the template uses it: some read many files.
    QString result = step.tmpl();
    const auto fill = [&result](const char* token, const std::function<QString()>& value) {
        const QString placeholder = QStringLiteral("{{%1}}").arg(QLatin1String(token));
        if (result.contains(placeholder))
            result.replace(placeholder, value());
    };

    fill("PROJECT_ROOT", [&] { return project.root(); });
    fill("FILE_LIST", [&] { return truncateForPrompt(project.fileListText(), 600, 18000); });
    fill("HISTORY", [&] { return conversation.historyText(12, 800); });
    fill("LAST_USER_MESSAGES", [&] { return conversation.recentRequests().join(QLatin1Char('\n')); });
    fill("OPEN_FILES", [&] { return context.openFilesText(); });
    fill("SUPPORTED_COMMANDS", [&] { return commandDocs(step.allowedCommands()); });
    fill("USER_QUERY", [&] { return userQuery; });
    fill("CURRENT_DATE", [&] { return date; });
    fill("TODAY", [&] { return date; });
    fill("CURRENT_DATETIME", [&] { return dateTime; });
    fill("LAST_ASSISTANTS_RESPONSE", [&] { return conversation.lastAssistantMessage(); });
    fill("SEARCH_RESULTS", [&] { return context.searchResultsText(); });
    fill("CONTEXT_CHAIN", [&] { return context.commandChainText(); });
    fill("FILE_DESCRIPTIONS", [&] { return project.descriptionsText(); });
    fill("PREVIOUS_STEPS", [&] { return previousStepsText(session.run()); });
    fill("OPEN_SEQUENCE_INFO", [&] { return sequenceInfo(session); });
    fill("PROJECT_STRUCTURE", [&] { return project.structureText(); });
    fill("TOKEN_COUNT", [&] { return QString::number(result.size() / 4 + 1); });

    // Unknown placeholders would only confuse the model.
    result.remove(QRegularExpression(QStringLiteral(R"(\{\{[^\}]+\}\})")));
    result.prepend(QStringLiteral("Runtime date context:\n"
                                  "- CURRENT_DATE: %1\n"
                                  "- CURRENT_DATETIME: %2\n"
                                  "- Interpret relative dates (today/now) using CURRENT_DATE.\n\n")
                           .arg(date, dateTime));
    return {result, userQuery};
}

QVector<PromptResolver::ResolverInfo> PromptResolver::availableResolvers()
{
    return {
        {"PROJECT_ROOT", QCoreApplication::translate("PromptResolver", "Project root folder")},
        {"FILE_LIST", QCoreApplication::translate("PromptResolver", "Project file list")},
        {"HISTORY", QCoreApplication::translate("PromptResolver", "Last 12 chat messages")},
        {"LAST_USER_MESSAGES", QCoreApplication::translate("PromptResolver", "Last 100 user messages from the chat")},
        {"OPEN_FILES", QCoreApplication::translate("PromptResolver", "Contents of files opened with OPEN_FILE")},
        {"SUPPORTED_COMMANDS", QCoreApplication::translate("PromptResolver", "Commands available in this step")},
        {"USER_QUERY", QCoreApplication::translate("PromptResolver", "Current user request")},
        {"CURRENT_DATE", QCoreApplication::translate("PromptResolver", "Current date (YYYY-MM-DD)")},
        {"TODAY", QCoreApplication::translate("PromptResolver", "Same as the current date (YYYY-MM-DD)")},
        {"CURRENT_DATETIME", QCoreApplication::translate("PromptResolver", "Current date/time with UTC offset")},
        {"LAST_ASSISTANTS_RESPONSE", QCoreApplication::translate("PromptResolver", "Last assistant response")},
        {"FILE_DESCRIPTIONS", QCoreApplication::translate("PromptResolver", "Add project file descriptions")},
        {"PREVIOUS_STEPS", QCoreApplication::translate("PromptResolver", "What was already done in the previous steps")},
        {"TOKEN_COUNT", QCoreApplication::translate("PromptResolver", "Token count of the current context")},
        {"SEARCH_RESULTS", QCoreApplication::translate("PromptResolver", "Short excerpts from SEARCH_WEB / SEARCH_PROJECT")},
        {"OPEN_SEQUENCE_INFO", QCoreApplication::translate("PromptResolver", "Sequences available in the template, as: number. description; sequenceName = \"file_name_without_.seq.json\"; summarize = true;")},
        {"CONTEXT_CHAIN", QCoreApplication::translate("PromptResolver", "Context chain of the executed commands with a visual status")},
        {"PROJECT_STRUCTURE", QCoreApplication::translate("PromptResolver", "Tree view of the project folder structure")},
    };
}
