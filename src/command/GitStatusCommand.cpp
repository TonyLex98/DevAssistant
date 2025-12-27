// GitStatusCommand.cpp
#include "GitStatusCommand.h"
#include <QProcess>
#include <QFileInfo>

static QString decodeGitOutput(const QByteArray& data)
{
#ifdef Q_OS_WIN
    return QString::fromLocal8Bit(data);
#else
    return QString::fromUtf8(data);
#endif
}

GitStatusCommand::GitStatusCommand(bool porcelain, const QString& path, const QString& descr)
{
    m_params["porcelain"] = porcelain;
    if (!path.trimmed().isEmpty())
        m_params["path"] = path;
    m_description = descr.isEmpty()
        ? QStringLiteral("Runs 'git status'. Example: { \"command\": \"GIT_STATUS\", \"params\": { \"porcelain\": true } }")
        : descr;
}

bool GitStatusCommand::apply(DevAssManager* devAss, QString* err)
{
    m_stdout.clear();
    m_stderr.clear();
    m_exitCode = 0;

    QProcess proc;
    if (devAss)
        proc.setWorkingDirectory(devAss->projectRoot().absolutePath());

    QStringList args;
    const bool porcelain = m_params.value("porcelain").toBool(true);
    args << "status";
    if (porcelain)
        args << "--porcelain=v1" << "-b";
    const QString path = m_params.value("path").toString().trimmed();
    if (!path.isEmpty()) {
        args << "--" << path;
    }

    proc.start("git", args);
    if (!proc.waitForStarted()) {
        if (err) *err = "GIT_STATUS: failed to start git";
        return false;
    }
    proc.waitForFinished(-1);
    m_stdout = decodeGitOutput(proc.readAllStandardOutput());
    m_stderr = decodeGitOutput(proc.readAllStandardError());
    m_exitCode = proc.exitCode();

    if (m_exitCode != 0) {
        if (err) *err = QString("GIT_STATUS: git exited with code %1").arg(m_exitCode);
        return false;
    }
    return true;
}

QJsonObject GitStatusCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString GitStatusCommand::getResult() const
{
    const bool ok = wasSuccessful();
    QString result = QString("GIT_STATUS\n[stdout]\n%1\n[stderr]\n%2\nexitCode=%3")
                         .arg(m_stdout.isEmpty() ? "No output" : m_stdout.trimmed())
                         .arg(m_stderr.isEmpty() ? "No errors" : m_stderr.trimmed())
                         .arg(ok ? 0 : m_exitCode);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

