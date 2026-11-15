#include "core/context/WorkingContext.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>
#include "core/commands/Command.h"
#include "core/util/Logging.h"
#include "core/util/json_utils.h"

namespace {

// An entry of the command history that is not a command the model ran (an attached image).
class NoteCommand final : public Command
{
public:
    NoteCommand(const QString& name, const QJsonObject& params, const QString& description, const QString& result)
        : m_name(name), m_params(params), m_description(description), m_result(result) {}
    QString name() const override { return m_name; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager*, QString*) override { return true; }
    QJsonObject toJson() const override
    {
        return {{QStringLiteral("command"), m_name}, {QStringLiteral("params"), m_params},
                {QStringLiteral("description"), m_description}};
    }
    QString getResult() const override { return m_result; }

private:
    QString m_name;
    QJsonObject m_params;
    QString m_description;
    QString m_result;
};

// Splits a file into lines; a final newline does not add an empty line.
QStringList splitLines(const QString& text)
{
    QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    if (text.endsWith(QLatin1Char('\n')) && !lines.isEmpty())
        lines.removeLast();
    return lines;
}

// Keeps the newest `max` entries.
QStringList newest(const QStringList& list, int max)
{
    if (list.size() <= max)
        return list;
    return list.mid(list.size() - max);
}

} // namespace

WorkingContext::WorkingContext(QObject* parent)
    : QObject(parent)
{
    m_files.setPathCallbacks(normalizeContextPath, displayContextPath);
    connect(&m_commands, &ContextManager::contextChanged, this, &WorkingContext::changed);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &WorkingContext::onFileChanged);
}

void WorkingContext::setProject(const QString& root, DevAssManager* manager)
{
    m_root = root;
    m_files.setProjectRoot(root);
    m_files.clearAll();
    m_files.clearConsoleLogs();
    m_files.clearExternalSources();
    m_commands.setDevAssManager(manager);
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    m_openedBy.clear();
    emit changed();
}

QString WorkingContext::normalize(const QString& path) const
{
    return normalizeContextPath(path, m_root);
}

void WorkingContext::watch(const QString& path)
{
    if (QFileInfo::exists(path) && !m_watcher.files().contains(path))
        m_watcher.addPath(path);
}

void WorkingContext::record(Command* command, const QString& result, const QString& sequenceId)
{
    if (!command)
        return;
    if (sequenceId.isEmpty())
        m_commands.addCommand(command, result);
    else
        m_commands.addCommandToSequence(command, sequenceId, result);
}

void WorkingContext::open(Command* command, const QString& sequenceId)
{
    const QJsonObject params = command->params();
    const QString rawPath = params.value(QStringLiteral("path")).toString();
    if (rawPath.isEmpty())
        return;
    const QString path = normalize(rawPath);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        record(command, QStringLiteral("ERROR: cannot open %1").arg(rawPath), sequenceId);
        return;
    }
    const QString text = QString::fromUtf8(file.readAll());
    const QStringList lines = splitLines(text);

    if (command->name() == QLatin1String("OPEN_FILE")) {
        m_files.setFullFileContext(path, lines);
        m_openedBy[path].insert(command->name());
        watch(path);
        record(command, QStringLiteral("OPEN_FILE: %1 (full content)").arg(rawPath), sequenceId);
        return;
    }

    int startLine = params.value(QStringLiteral("startLine")).toInt(-1);
    const int endLine = params.value(QStringLiteral("endLine")).toInt(-1);
    QStringList snippet;
    if (startLine >= 1 && endLine >= startLine) {
        const int last = startLine > lines.size() ? startLine : endLine;
        if (last > lines.size()) {
            record(command,
                   QStringLiteral("ERROR: Cannot OPEN_FILE_REGION %1: line %2 is beyond the end of the file (%3 lines). "
                                  "Line numbers start at 1.")
                           .arg(rawPath).arg(last).arg(lines.size()),
                   sequenceId);
            return;
        }
        snippet = lines.mid(startLine - 1, endLine - startLine + 1);
    } else {
        // A regular expression that marks the region.
        const QString region = params.value(QStringLiteral("region")).toString();
        if (region.isEmpty())
            return;
        const QRegularExpression re(region, QRegularExpression::DotMatchesEverythingOption
                                                    | QRegularExpression::MultilineOption);
        const QRegularExpressionMatch match = re.match(text);
        if (!match.hasMatch()) {
            record(command, QStringLiteral("OPEN_FILE_REGION: nothing in %1 matches the region").arg(rawPath), sequenceId);
            return;
        }
        startLine = text.left(match.capturedStart()).count(QLatin1Char('\n')) + 1;
        snippet = splitLines(match.captured(0));
    }
    if (snippet.isEmpty())
        return;

    m_files.addRegionContext(path, startLine, snippet);
    m_openedBy[path].insert(command->name());
    watch(path);
    record(command,
           QStringLiteral("OPEN_FILE_REGION: %1\n%2").arg(rawPath, m_files.formatNumberedLines(snippet, startLine)),
           sequenceId);
}

void WorkingContext::forgetChangedFiles(Command* command)
{
    const QString name = command->name();
    const QJsonObject p = command->params();
    const auto forget = [this](const QString& path) {
        if (!path.isEmpty())
            m_files.clearContextForPath(path);
    };

    static const QStringList oneFile = {
        QStringLiteral("EDIT_IN_RANGE"), QStringLiteral("REPLACE_IN_FILE"), QStringLiteral("DELETE_FILE"),
        QStringLiteral("CREATE_FILE"), QStringLiteral("APPEND_TO_FILE"), QStringLiteral("INSERT_AT_LINE"),
        QStringLiteral("GIT_RESTORE_FILE")
    };
    if (oneFile.contains(name)) {
        forget(p.value(QStringLiteral("path")).toString());
    } else if (name == QLatin1String("RENAME_FILE")) {
        forget(p.value(QStringLiteral("oldPath")).toString());
        forget(p.value(QStringLiteral("newPath")).toString());
    } else if (name == QLatin1String("COPY_FILE")) {
        forget(p.value(QStringLiteral("to")).toString());
    } else if (name == QLatin1String("APPLY_PATCH") || name == QLatin1String("UNDO_LAST_CHANGE")) {
        m_files.clearAll();  // which files changed is not known here
    } else if (name == QLatin1String("RUN_SHELL")) {
        // A new run of the same command replaces the old output.
        m_files.removeConsoleLogsWithPrefix(QStringLiteral("==== RUN_SHELL: ") + p.value(QStringLiteral("command")).toString());
    } else if (name == QLatin1String("RUN_PYTHON")) {
        const QString script = p.value(QStringLiteral("scriptPath")).toString();
        m_files.removeConsoleLogsWithPrefix(QStringLiteral("==== RUN_PYTHON: ")
                                            + (script.isEmpty() ? QStringLiteral("inline code") : script));
    }
}

void WorkingContext::addConsoleOutput(const QString& title, const QString& stdOut, const QString& stdErr, int exitCode)
{
    const QString heading = title.isEmpty() ? QStringLiteral("Console") : title;
    QString block = QStringLiteral("==== %1 ====\n").arg(heading);
    if (!stdOut.trimmed().isEmpty())
        block += QStringLiteral("[stdout]\n") + stdOut.trimmed() + QLatin1Char('\n');
    if (!stdErr.trimmed().isEmpty())
        block += QStringLiteral("[stderr]\n") + stdErr.trimmed() + QLatin1Char('\n');
    block += QStringLiteral("exitCode=%1").arg(exitCode);

    // The same program run again replaces its earlier output.
    m_files.removeConsoleLogsWithPrefix(QStringLiteral("==== %1 ====").arg(heading));
    const QStringList logs = newest(m_files.consoleLogs() + QStringList{block.trimmed()}, kMaxBlocks);
    m_files.clearConsoleLogs();
    for (const QString& log : logs)
        m_files.appendConsoleLog(log);
}

void WorkingContext::addExternal(const QString& block)
{
    if (block.trimmed().isEmpty())
        return;
    const QStringList sources = newest(m_files.externalSources() + QStringList{block.trimmed()}, kMaxBlocks);
    m_files.clearExternalSources();
    for (const QString& s : sources)
        m_files.appendExternalSource(s);
}

void WorkingContext::addImages(const QStringList& paths, const QStringList& summaries)
{
    for (int i = 0; i < paths.size(); ++i) {
        const QString fileName = QFileInfo(paths.at(i)).fileName();
        const QString summary = i < summaries.size() ? summaries.at(i).trimmed() : QString();
        QJsonObject params{{QStringLiteral("path"), paths.at(i)}};
        if (!summary.isEmpty())
            params.insert(QStringLiteral("summary"), summary);
        const QString result = QStringLiteral("=== IMAGE: %1 ===\n%2").arg(fileName, summary.isEmpty() ? paths.at(i) : summary);
        NoteCommand note(QStringLiteral("USER_IMAGE"), params, QStringLiteral("User attached an image"), result);
        m_commands.addCommand(&note, result);
    }
}

void WorkingContext::clear(bool everything)
{
    m_files.clearAll();
    m_files.clearConsoleLogs();
    m_files.clearExternalSources();
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    m_openedBy.clear();
    if (everything)
        m_commands.clearAll();
    else
        m_commands.clearActiveContextOnly();
    emit changed();
}

void WorkingContext::observeRequest(const QString& text)
{
    m_commands.observeUserQuery(text);
}

QString WorkingContext::openFilesText()
{
    m_files.rebuildContextCache();
    return m_files.contextContent();
}

QString WorkingContext::searchResultsText() const
{
    return m_files.externalSources().join(QStringLiteral("\n\n"));
}

QString WorkingContext::commandChainText() const
{
    QStringList parts;
    for (const CommandContextInfo& info : m_commands.getAllCommands()) {
        const QString text = info.result.isEmpty() ? info.description : info.result;
        if (text.isEmpty())
            continue;
        QString state;
        switch (info.state) {
        case CommandState::Active: state = QStringLiteral(" [ACTIVE]"); break;
        case CommandState::Invalidated: state = QStringLiteral(" [INVALIDATED: %1]").arg(info.reason); break;
        case CommandState::Removed: state = QStringLiteral(" [REMOVED]"); break;
        }
        parts << text + state;
    }
    return parts.join(QStringLiteral("\n\n"));
}

QString WorkingContext::activeCommandsText() const
{
    return m_commands.generateContextText();
}

WorkingContext::Snapshot WorkingContext::snapshotSince(const QDateTime& since) const
{
    Snapshot out;
    if (!since.isValid())
        return out;
    QSet<QString> ids;
    for (const CommandContextInfo& cmd : m_commands.getAllCommands()) {
        if (cmd.timestamp >= since) {
            out.commands << cmd;
            ids.insert(cmd.id);
        }
    }
    if (out.commands.isEmpty())
        return out;
    for (const CommandSequenceInfo& seq : m_commands.getCommandSequences()) {
        bool include = seq.timestamp >= since;
        for (int i = 0; !include && i < seq.commandIds.size(); ++i)
            include = ids.contains(seq.commandIds.at(i));
        if (include)
            out.sequences << seq;
    }
    return out;
}

void WorkingContext::onFileChanged(const QString& path)
{
    // Editors often replace a file (delete + write): watch it again once that is done.
    m_watcher.removePath(path);
    QTimer::singleShot(100, this, [this, path]() {
        watch(path);
        if (m_openedBy.remove(path) > 0) {
            qCDebug(logCore) << "File changed outside the program, dropped from the context:" << path;
            m_files.clearContextForPath(path);
        }
        m_commands.cleanupForPath(path, QStringLiteral("Context invalidated: %1 changed externally").arg(path));
        emit changed();
    });
}
