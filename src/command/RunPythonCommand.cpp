// RunPythonCommand.cpp
#include "RunPythonCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

static QString decodeProcessOutput(const QByteArray& data)
{
#ifdef Q_OS_WIN
    return QString::fromLocal8Bit(data);
#else
    return QString::fromUtf8(data);
#endif
}

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
        ? QString("Runs a Python script or inline Python code. "
                  "Example: { \"command\": \"RUN_PYTHON\", \"params\": { \"code\": \"print(1+1)\" } }")
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
        if (!info.isAbsolute() && devAss) {
            path = devAss->projectRoot().filePath(path);
            // Обновляем путь в параметрах для отображения в результатах
            m_params["scriptPath"] = path;
        }
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
    m_stdout = decodeProcessOutput(proc.readAllStandardOutput());
    m_stderr = decodeProcessOutput(proc.readAllStandardError());
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

QString RunPythonCommand::getResult() const
{
    QString scriptOrCode;
    if (m_params.contains("scriptPath")) {
        scriptOrCode = m_params.value("scriptPath").toString();
    } else if (m_params.contains("code")) {
        scriptOrCode = "inline code";
    } else {
        scriptOrCode = "unknown";
    }
    
    QString result = QString("RUN_PYTHON: %1\n").arg(scriptOrCode);
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
