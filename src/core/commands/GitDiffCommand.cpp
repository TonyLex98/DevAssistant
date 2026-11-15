// GitDiffCommand.cpp
#include "core/commands/GitDiffCommand.h"
#include <QProcess>

static QString decodeGitOutput(const QByteArray& data)
{
#ifdef Q_OS_WIN
    return QString::fromLocal8Bit(data);
#else
    return QString::fromUtf8(data);
#endif
}

GitDiffCommand::GitDiffCommand(bool staged, int contextLines, const QString& path, const QString& descr)
{
    m_params["staged"] = staged;
    if (contextLines >= 0)
        m_params["contextLines"] = contextLines;
    if (!path.trimmed().isEmpty())
        m_params["path"] = path;
    m_description = descr.isEmpty()
        ? QStringLiteral("Runs 'git diff'. Example: { \"command\": \"GIT_DIFF\", \"params\": { \"staged\": false } }")
        : descr;
}

bool GitDiffCommand::apply(DevAssManager* devAss, QString* err)
{
    m_stdout.clear();
    m_stderr.clear();
    m_exitCode = 0;

    QProcess proc;
    if (devAss)
        proc.setWorkingDirectory(devAss->projectRoot().absolutePath());

    QStringList args;
    args << "diff";
    if (m_params.value("staged").toBool(false))
        args << "--staged";

    const int contextLines = m_params.value("contextLines").toInt(-1);
    if (contextLines >= 0)
        args << QString("-U%1").arg(contextLines);

    const QString path = m_params.value("path").toString().trimmed();
    if (!path.isEmpty())
        args << "--" << path;

    proc.start("git", args);
    if (!proc.waitForStarted()) {
        if (err) *err = "GIT_DIFF: failed to start git";
        return false;
    }
    proc.waitForFinished(-1);
    m_stdout = decodeGitOutput(proc.readAllStandardOutput());
    m_stderr = decodeGitOutput(proc.readAllStandardError());
    m_exitCode = proc.exitCode();

    if (m_exitCode != 0) {
        if (err) *err = QString("GIT_DIFF: git exited with code %1").arg(m_exitCode);
        return false;
    }
    return true;
}

QJsonObject GitDiffCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString GitDiffCommand::getResult() const
{
    const bool ok = wasSuccessful();
    QString result = QString("GIT_DIFF\n[stdout]\n%1\n[stderr]\n%2\nexitCode=%3")
                         .arg(m_stdout.isEmpty() ? "No output" : m_stdout.trimmed())
                         .arg(m_stderr.isEmpty() ? "No errors" : m_stderr.trimmed())
                         .arg(ok ? 0 : m_exitCode);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

