// RunShellCommand.cpp
#include "RunShellCommand.h"
#include <QProcess>
#include <QDebug>

static QString decodeProcessOutput(const QByteArray& data)
{
#ifdef Q_OS_WIN
    return QString::fromLocal8Bit(data);
#else
    return QString::fromUtf8(data);
#endif
}

RunShellCommand::RunShellCommand(const QString& command,
                                 const QString& descr)
{
    m_params["command"] = command;
    m_description = descr.isEmpty()
        ? QString("Executes a shell command. "
                  "Example: { \"command\": \"RUN_SHELL\", \"params\": { \"command\": \"ls -la\" } }")
        : descr;
}

bool RunShellCommand::apply(DevAssManager* devAss, QString* err)
{
    if (!m_params.contains("command")) {
        if (err) *err = "Команда не задана";
        return false;
    }

    QProcess proc;
    QString cmd = m_params["command"].toString();

    // Установить рабочую директорию из devAssManager, если возможно
    if (devAss) {
        QDir projectRoot = devAss->projectRoot();
        if (!projectRoot.isEmpty() && projectRoot.exists()) {
            proc.setWorkingDirectory(projectRoot.absolutePath());
        }
    }

    // Для *nix будем использовать /bin/sh -c
#ifdef Q_OS_WIN
    proc.start("cmd", {"/C", cmd});
#else
    proc.start("/bin/sh", {"-c", cmd});
#endif

    if (!proc.waitForStarted()) {
        if (err) *err = "Не удалось запустить команду";
        return false;
    }
    proc.waitForFinished(-1);
    m_stdout = decodeProcessOutput(proc.readAllStandardOutput());
    m_stderr = decodeProcessOutput(proc.readAllStandardError());
    m_exitCode = proc.exitCode();

    if (m_exitCode != 0) {
        if (err) *err = QString("Команда завершилась с кодом %1").arg(m_exitCode);
        return false;
    }
    return true;
}

QJsonObject RunShellCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = m_description;
    return obj;
}

QString RunShellCommand::getResult() const
{
    QString result = QString("RUN_SHELL: %1\n").arg(m_params["command"].toString());
    if (!m_stdout.isEmpty()) {
        result += QString("[stdout]\n%1\n").arg(m_stdout);
    } else {
        result += "[stdout]\nNo output\n";
    }
    if (!m_stderr.isEmpty()) {
        result += QString("[stderr]\n%1\n").arg(m_stderr);
    } else {
        result += "[stderr]\nNo errors\n";
    }
    result += QString("exitCode=%1").arg(m_exitCode);
    
    // Add error message if command failed
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}
