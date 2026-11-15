// GitRestoreFileCommand.cpp
#include "core/commands/GitRestoreFileCommand.h"
#include <QProcess>

static QString decodeGitOutput(const QByteArray& data)
{
#ifdef Q_OS_WIN
    return QString::fromLocal8Bit(data);
#else
    return QString::fromUtf8(data);
#endif
}

GitRestoreFileCommand::GitRestoreFileCommand(const QString& path, bool staged, bool worktree, const QString& descr)
{
    m_params["path"] = path;
    m_params["staged"] = staged;
    m_params["worktree"] = worktree;
    m_description = descr.isEmpty()
        ? QStringLiteral("Restores a file from git (like 'git restore'). "
                         "Note: this change is performed by git; UNDO_LAST_CHANGE may not revert it. "
                         "Example: { \"command\": \"GIT_RESTORE_FILE\", \"params\": { \"path\": \"src/main.cpp\", \"staged\": false } }")
        : descr;
}

bool GitRestoreFileCommand::runGit(const QStringList& args, DevAssManager* devAss, QString* err)
{
    QProcess proc;
    if (devAss)
        proc.setWorkingDirectory(devAss->projectRoot().absolutePath());

    proc.start("git", args);
    if (!proc.waitForStarted()) {
        if (err) *err = "GIT_RESTORE_FILE: failed to start git";
        return false;
    }
    proc.waitForFinished(-1);
    m_stdout = decodeGitOutput(proc.readAllStandardOutput());
    m_stderr = decodeGitOutput(proc.readAllStandardError());
    m_exitCode = proc.exitCode();
    return (m_exitCode == 0);
}

bool GitRestoreFileCommand::apply(DevAssManager* devAss, QString* err)
{
    m_stdout.clear();
    m_stderr.clear();
    m_exitCode = 0;

    const QString path = m_params.value("path").toString().trimmed();
    if (path.isEmpty()) {
        if (err) *err = "GIT_RESTORE_FILE: path is empty";
        return false;
    }

    const bool staged = m_params.value("staged").toBool(false);
    const bool worktree = m_params.value("worktree").toBool(true);

    if (!staged && !worktree) {
        if (err) *err = "GIT_RESTORE_FILE: nothing to do (staged=false and worktree=false)";
        return false;
    }

    // Prefer 'git restore' (git >= 2.23).
    QStringList restoreArgs;
    restoreArgs << "restore";
    if (staged)
        restoreArgs << "--staged";
    if (staged && worktree)
        restoreArgs << "--worktree";
    restoreArgs << "--" << path;

    if (runGit(restoreArgs, devAss, err))
        return true;

    // Fallback for older git: use checkout/reset.
    if (staged) {
        if (runGit(QStringList() << "reset" << "HEAD" << "--" << path, devAss, err) && !worktree)
            return true;
        if (runGit(QStringList() << "reset" << "HEAD" << "--" << path, devAss, err) && worktree) {
            if (runGit(QStringList() << "checkout" << "--" << path, devAss, err))
                return true;
        }
    } else {
        if (runGit(QStringList() << "checkout" << "--" << path, devAss, err))
            return true;
    }

    if (err) *err = QString("GIT_RESTORE_FILE: git failed (exitCode=%1)").arg(m_exitCode);
    return false;
}

QJsonObject GitRestoreFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString GitRestoreFileCommand::getResult() const
{
    const bool ok = wasSuccessful();
    const QString path = m_params.value("path").toString();
    QString result = QString("GIT_RESTORE_FILE: %1\n[stdout]\n%2\n[stderr]\n%3\nexitCode=%4")
                         .arg(path)
                         .arg(m_stdout.isEmpty() ? "No output" : m_stdout.trimmed())
                         .arg(m_stderr.isEmpty() ? "No errors" : m_stderr.trimmed())
                         .arg(ok ? 0 : m_exitCode);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}
