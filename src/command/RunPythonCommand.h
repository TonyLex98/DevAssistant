// RunPythonCommand.h
#pragma once
#include "Command.h"
#include <QProcess>

class RunPythonCommand : public Command
{
public:
    /* scriptPath – путь к файлу‑скрипту.
       inlineCode – код для запуска через `-c`.  Если оба аргумента пустые – команда будет невалидной.
    */
    RunPythonCommand(const QString& scriptPath = QString(),
                     const QByteArray& inlineCode = QByteArray(),
                     const QString& descr = QString());

    QString name() const override { return "RUN_PYTHON"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }

    /* Запускает `python` либо скрипт из файла, либо код через `-c`. */
    bool apply(DevAssManager* devAss, QString* err) override;

    QJsonObject toJson() const override;

private:
    QJsonObject m_params;
    QString m_description;
};
