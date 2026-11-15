#include "core/commands/CommandPolicy.h"

#include "core/commands/Command.h"
#include "core/project/PathGuard.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace {

const QSet<QString>& readOnlyCommands()
{
    static const QSet<QString> names = {
        QStringLiteral("OPEN_FILE"),          QStringLiteral("OPEN_FILE_REGION"),
        QStringLiteral("SEARCH_PROJECT"),     QStringLiteral("SEARCH_PROJECT_CASE_INSENSITIVE"),
        QStringLiteral("LIST_FILES_IN_FOLDER"), QStringLiteral("SEARCH_WEB"),
        QStringLiteral("SEARCH_DOCS"),
        QStringLiteral("GIT_STATUS"),         QStringLiteral("GIT_DIFF"),
        QStringLiteral("CONTEXT_SHOW"),       QStringLiteral("REASONING"),
        QStringLiteral("SHOW_RESULT"),        QStringLiteral("ASK_USER"),
        QStringLiteral("OPEN_SEQUENCE"),      QStringLiteral("CALL_SEQUENCE"),
    };
    return names;
}

const QSet<QString>& modifyingCommands()
{
    static const QSet<QString> names = {
        QStringLiteral("CREATE_FILE"),   QStringLiteral("CREATE_DIR"),
        QStringLiteral("EDIT_IN_RANGE"), QStringLiteral("INSERT_AT_LINE"),
        QStringLiteral("APPEND_TO_FILE"), QStringLiteral("REPLACE_IN_FILE"),
        QStringLiteral("APPLY_PATCH"),   QStringLiteral("RENAME_FILE"),
        QStringLiteral("COPY_FILE"),     QStringLiteral("WRITE_DESCRIPTION"),
        QStringLiteral("UNDO_LAST_CHANGE"), QStringLiteral("CONTEXT_CLEAR"),
        QStringLiteral("CLIPBOARD_SET"),
    };
    return names;
}

QString relative(const QString& path, const QString& projectRoot)
{
    if (projectRoot.isEmpty())
        return QDir::toNativeSeparators(path);
    const QString abs = QFileInfo(path).isAbsolute()
            ? QDir::cleanPath(path)
            : QDir::cleanPath(QDir(projectRoot).absoluteFilePath(path));
    const QString rel = QDir(projectRoot).relativeFilePath(abs);
    const bool outside = rel == QLatin1String("..") || rel.startsWith(QLatin1String("../"))
            || QDir::isAbsolutePath(rel);   // another drive
    return QDir::toNativeSeparators(outside ? abs : rel);
}

QString lineCount(const QString& text)
{
    const int lines = text.isEmpty() ? 0 : text.count(QLatin1Char('\n')) + 1;
    return QCoreApplication::translate("CommandPolicy", "%n line(s)", nullptr, lines);
}

QString clipped(const QString& text, int maxChars)
{
    return text.size() <= maxChars ? text : text.left(maxChars) + QStringLiteral("…");
}

} // namespace

namespace CommandPolicy {

Risk riskOf(const QString& commandName)
{
    const QString name = commandName.trimmed().toUpper();
    if (readOnlyCommands().contains(name))
        return Risk::ReadOnly;
    if (modifyingCommands().contains(name))
        return Risk::Modifying;
    // RUN_SHELL, RUN_PYTHON, OPEN_APP, OPEN_URL, DELETE_FILE, DELETE_DIR, GIT_RESTORE_FILE,
    // CLIPBOARD_GET and anything unknown.
    return Risk::Dangerous;
}

QString riskLabel(Risk risk)
{
    switch (risk) {
    case Risk::ReadOnly:  return QCoreApplication::translate("CommandPolicy", "read-only");
    case Risk::Modifying: return QCoreApplication::translate("CommandPolicy", "changes files");
    case Risk::Dangerous: return QCoreApplication::translate("CommandPolicy", "dangerous");
    }
    return QString();
}

bool isReadOnlyBatch(const QList<Command*>& commands)
{
    if (commands.isEmpty())
        return false;
    for (const Command* c : commands) {
        if (!c || riskOf(c->name()) != Risk::ReadOnly)
            return false;
    }
    return true;
}

const QStringList& pathParamNames()
{
    static const QStringList names = {
        QStringLiteral("path"),       QStringLiteral("oldPath"),    QStringLiteral("newPath"),
        QStringLiteral("from"),       QStringLiteral("to"),         QStringLiteral("scriptPath"),
        QStringLiteral("cwd"),        QStringLiteral("folderPath"), QStringLiteral("sourceFilePath"),
    };
    return names;
}

bool check(const QString& name, const QJsonObject& params, const QString& projectRoot,
           const QStringList& allowed, QString* errorOut)
{
    const QString upper = name.trimmed().toUpper();
    // A step's allowed list limits what may change or run; read-only commands (open, search,
    // show the result, ask the user) stay available so the model can always finish the step.
    if (!allowed.isEmpty() && riskOf(upper) != Risk::ReadOnly) {
        bool found = false;
        for (const QString& a : allowed) {
            if (a.trimmed().compare(upper, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (errorOut)
                *errorOut = QCoreApplication::translate("CommandPolicy", "%1 is not allowed in this step. Allowed: %2.")
                        .arg(upper, allowed.join(QStringLiteral(", ")));
            return false;
        }
    }

    for (const QString& key : pathParamNames()) {
        const QJsonValue v = params.value(key);
        if (!v.isString() || v.toString().trimmed().isEmpty())
            continue;
        QString reason;
        if (!PathGuard::resolveInside(projectRoot, v.toString(), nullptr, &reason)) {
            if (errorOut)
                *errorOut = QStringLiteral("%1 %2: %3").arg(upper, key, reason);
            return false;
        }
    }
    return true;
}

bool check(const Command* command, const QString& projectRoot,
           const QStringList& allowed, QString* errorOut)
{
    if (!command) {
        if (errorOut)
            *errorOut = QCoreApplication::translate("CommandPolicy", "Empty command.");
        return false;
    }
    return check(command->name(), command->params(), projectRoot, allowed, errorOut);
}

QString details(const QString& name, const QJsonObject& params, const QString& projectRoot)
{
    const QString upper = name.trimmed().toUpper();
    const auto str = [&params](const char* key) { return params.value(QLatin1String(key)).toString(); };
    const auto path = [&](const char* key) { return relative(str(key), projectRoot); };

    if (upper == QLatin1String("RUN_SHELL")) {
        QString out = QStringLiteral("> ") + clipped(str("command"), 600);
        if (!str("cwd").isEmpty())
            out += QStringLiteral("\n") + QCoreApplication::translate("CommandPolicy", "in %1").arg(path("cwd"));
        return out;
    }
    if (upper == QLatin1String("RUN_PYTHON")) {
        if (!str("scriptPath").isEmpty())
            return QCoreApplication::translate("CommandPolicy", "python %1").arg(path("scriptPath"));
        return QCoreApplication::translate("CommandPolicy", "Python code, %1:\n%2").arg(lineCount(str("code")), clipped(str("code"), 600));
    }
    if (upper == QLatin1String("OPEN_APP")) {
        QStringList args;
        for (const QJsonValue& a : params.value(QStringLiteral("args")).toArray())
            args << a.toString();
        if (args.isEmpty() && params.value(QStringLiteral("args")).isString())
            args << str("args");
        return QStringLiteral("%1 %2").arg(str("app"), args.join(QLatin1Char(' '))).trimmed();
    }
    if (upper == QLatin1String("OPEN_URL"))
        return str("url");
    if (upper == QLatin1String("DELETE_DIR")) {
        return params.value(QStringLiteral("recursive")).toBool()
                ? QCoreApplication::translate("CommandPolicy", "%1 and everything in it").arg(path("path"))
                : QCoreApplication::translate("CommandPolicy", "%1 (only if empty)").arg(path("path"));
    }
    if (upper == QLatin1String("RENAME_FILE") || upper == QLatin1String("COPY_FILE")) {
        const QString from = !str("oldPath").isEmpty() ? path("oldPath") : path("from");
        const QString to = !str("newPath").isEmpty() ? path("newPath") : path("to");
        return QStringLiteral("%1 → %2").arg(from, to);
    }
    if (upper == QLatin1String("EDIT_IN_RANGE") || upper == QLatin1String("OPEN_FILE_REGION")) {
        const QString start = params.value(QStringLiteral("startLine")).toVariant().toString();
        const QString end = params.value(QStringLiteral("endLine")).toVariant().toString();
        QString out = QCoreApplication::translate("CommandPolicy", "%1, lines %2–%3").arg(path("path"), start, end);
        if (params.contains(QStringLiteral("content")))
            out += QStringLiteral("\n") + QCoreApplication::translate("CommandPolicy", "new text: %1").arg(lineCount(str("content")));
        return out;
    }
    if (upper == QLatin1String("CREATE_FILE") || upper == QLatin1String("APPEND_TO_FILE")
            || upper == QLatin1String("INSERT_AT_LINE")) {
        return QCoreApplication::translate("CommandPolicy", "%1, %2").arg(path("path"), lineCount(str("content")));
    }
    if (upper == QLatin1String("APPLY_PATCH"))
        return QCoreApplication::translate("CommandPolicy", "patch, %1").arg(lineCount(str("patch")));
    if (upper.startsWith(QLatin1String("SEARCH_")))
        return QStringLiteral("\"%1\"").arg(clipped(str("query").isEmpty() ? str("pattern") : str("query"), 200));
    if (!str("path").isEmpty())
        return path("path");

    // Anything else: compact parameters, long text shortened.
    QJsonObject shown;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (it.key() == QLatin1String("description"))
            continue;
        shown.insert(it.key(), it.value().isString() ? QJsonValue(clipped(it.value().toString(), 160)) : it.value());
    }
    return shown.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(shown).toJson(QJsonDocument::Compact));
}

} // namespace CommandPolicy
