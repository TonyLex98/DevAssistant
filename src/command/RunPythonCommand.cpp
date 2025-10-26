// RunPythonCommand.cpp
#include "RunPythonCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

RunPythonCommand::RunPythonCommand(const QString& scriptPath,
                                   const QByteArray& inlineCode,
                                   const QString& descr)
{
    if (!scriptPath.isEmpty()) {
        m_params["scriptPath"] = scriptPath;
    }
    if (!inlineCode.isEmpty()) {
        m_params["code"] = QString::fromUtf8(inlineCode);
    }
    m_description = descr.isEmpty()
        ? (scriptPath.isEmpty()
               ? QString("Запустить Python-код")
               : QString("Запустить скрипт «%1»").arg(scriptPath))
        : descr;
}

bool RunPythonCommand::apply(DevAssManager* devAss, QString* err)
{
    QProcess proc;
    QString program = "python";
    QStringList args;
    const QString rawScript = m_params.value("scriptPath").toString().trimmed();
    const QString code = m_params.value("code").toString();
    const bool hasCode = !code.trimmed().isEmpty();

    if (!rawScript.isEmpty()) {
        QString path = rawScript;
        QFileInfo info(path);
        if (!info.isAbsolute() && devAss)
            path = devAss->projectRoot().filePath(path);
        if (!QFile::exists(path)) {
            if (err) *err = QString("Файл скрипта %1 не найден").arg(path);
            return false;
        }
        args << path;
    } else if (hasCode) {
        args << "-c" << code;
    } else {
        if (err) *err = "Ни скрипта, ни кода не указано";
        return false;
    }

    proc.start(program, args);

    if (!proc.waitForStarted()) {
        if (err) *err = "Не удалось запустить Python";
        return false;
    }
    proc.waitForFinished(-1);
    m_stdout = QString::fromUtf8(proc.readAllStandardOutput());
    m_stderr = QString::fromUtf8(proc.readAllStandardError());
    m_exitCode = proc.exitCode();

    if (m_exitCode != 0) {
        if (err) *err = QString("Python завершился с кодом %1").arg(m_exitCode);
        return false;
    }
    return true;
}

QJsonObject RunPythonCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = m_description;
    return obj;
}
