// RunPythonCommand.cpp
#include "RunPythonCommand.h"
#include <QFile>
#include <QDebug>

RunPythonCommand::RunPythonCommand(const QString& scriptPath,
                                   const QByteArray& inlineCode,
                                   const QString& descr)
{
    // Сохраняем параметры. Если scriptPath не пустой – считаем, что это файл.
    // Иначе храним код.
    if (!scriptPath.isEmpty()) {
        m_params["scriptPath"] = scriptPath;
    }
    if (!inlineCode.isEmpty()) {
        m_params["code"] = QString::fromUtf8(inlineCode);
    }
    m_description = descr.isEmpty()
        ? (scriptPath.isEmpty()
               ? QString("Запустить Python‑код")   // inline
               : QString("Запустить скрипт «%1»").arg(scriptPath))
        : descr;
}

bool RunPythonCommand::apply(DevAssManager* /*devAss*/, QString* err)
{
    QProcess proc;
    QString program = "python";

    if (m_params.contains("scriptPath")) {
        QString path = m_params["scriptPath"].toString();
        if (!QFile::exists(path)) {
            if (err) *err = QString("Файл скрипта %1 не найден").arg(path);
            return false;
        }
        proc.start(program, {path});
    } else if (m_params.contains("code")) {
        proc.start(program, {"-c", m_params["code"].toString()});
    } else {
        if (err) *err = "Ни скрипта, ни кода не указано";
        return false;
    }

    // Запускаем синхронно (до завершения)
    if (!proc.waitForStarted()) {
        if (err) *err = "Не удалось запустить Python";
        return false;
    }
    proc.waitForFinished(-1); // бесконечный таймаут

    int exitCode = proc.exitCode();
    if (exitCode != 0) {
        if (err) *err = QString("Python завершился с кодом %1").arg(exitCode);
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
