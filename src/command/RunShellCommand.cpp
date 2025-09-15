// RunShellCommand.cpp
#include "RunShellCommand.h"
#include <QProcess>
#include <QDebug>

RunShellCommand::RunShellCommand(const QString& command,
                                 const QString& descr)
{
    m_params["command"] = command;
    m_description = descr.isEmpty()
        ? QString("Запустить команду «%1»").arg(command)
        : descr;
}

bool RunShellCommand::apply(DevAssManager* /*devAss*/, QString* err)
{
    if (!m_params.contains("command")) {
        if (err) *err = "Команда не задана";
        return false;
    }

    QProcess proc;
    QString cmd = m_params["command"].toString();

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

    int exitCode = proc.exitCode();
    if (exitCode != 0) {
        if (err) *err = QString("Команда завершилась с кодом %1").arg(exitCode);
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
