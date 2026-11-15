#include "core/commands/OpenAppCommand.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>

namespace {
QStringList splitCommandCompat(const QString& text)
{
    QStringList args;
    QString current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            continue;
        }
        if (ch == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && ch.isSpace()) {
            if (!current.isEmpty()) {
                args << current;
                current.clear();
            }
            continue;
        }

        if (ch == '\\' && i + 1 < text.size()) {
            const QChar next = text.at(i + 1);
            if (next == '"' || next == '\'' || next == '\\') {
                current += next;
                ++i;
                continue;
            }
        }
        current += ch;
    }

    if (!current.isEmpty())
        args << current;
    return args;
}
}

OpenAppCommand::OpenAppCommand(const QString& app,
                               const QJsonValue& args,
                               const QString& cwd,
                               bool detached,
                               int waitMs,
                               const QString& descr)
{
    m_params["app"] = app;
    if (!args.isUndefined() && !args.isNull())
        m_params["args"] = args;
    if (!cwd.trimmed().isEmpty())
        m_params["cwd"] = cwd;
    m_params["detached"] = detached;
    m_params["waitMs"] = waitMs;

    m_description = descr.isEmpty()
        ? QStringLiteral("Launches a desktop application. "
                         "Example: { \"command\": \"OPEN_APP\", \"params\": { \"app\": \"notepad.exe\", \"args\": [\"README.md\"] } }")
        : descr;
}

bool OpenAppCommand::apply(DevAssManager* devAss, QString* err)
{
    m_started = false;
    m_finished = false;
    m_exitCode = 0;
    m_pid = 0;

    QString app = m_params.value("app").toString().trimmed();
    if (app.isEmpty()) {
        if (err) *err = "OPEN_APP: app is empty";
        return false;
    }

    QString cwd = m_params.value("cwd").toString().trimmed();
    if (!cwd.isEmpty() && devAss && !QFileInfo(cwd).isAbsolute()) {
        cwd = QDir(devAss->projectRoot().absolutePath()).filePath(cwd);
        m_params["cwd"] = cwd;
    }

    QStringList args;
    const QJsonValue argsValue = m_params.value("args");
    if (argsValue.isArray()) {
        const QJsonArray arr = argsValue.toArray();
        for (const QJsonValue& v : arr)
            args << v.toString();
    } else if (argsValue.isString()) {
        args = splitCommandCompat(argsValue.toString());
    }

    const bool detached = m_params.value("detached").toBool(true);
    m_detachedLaunch = detached;

    if (detached) {
        m_started = QProcess::startDetached(app, args, cwd, &m_pid);
    } else {
        int waitMs = m_params.value("waitMs").toInt(15000);
        if (waitMs < -1)
            waitMs = 15000;

        QProcess proc;
        if (!cwd.isEmpty())
            proc.setWorkingDirectory(cwd);
        proc.start(app, args);
        m_started = proc.waitForStarted(5000);
        if (!m_started) {
            if (err) *err = QString("OPEN_APP: failed to start '%1'").arg(app);
            return false;
        }

        m_pid = proc.processId();
        m_finished = proc.waitForFinished(waitMs);
        if (!m_finished) {
            proc.kill();
            proc.waitForFinished(2000);
            if (err) *err = QString("OPEN_APP: timed out after %1 ms for '%2'").arg(waitMs).arg(app);
            return false;
        }

        m_exitCode = proc.exitCode();
        if (proc.exitStatus() != QProcess::NormalExit || m_exitCode != 0) {
            if (err) *err = QString("OPEN_APP: process exited abnormally (exitCode=%1) for '%2'")
                                .arg(m_exitCode).arg(app);
            return false;
        }
    }

    if (!m_started) {
        if (err) *err = QString("OPEN_APP: failed to start '%1'").arg(app);
        return false;
    }
    return true;
}

QJsonObject OpenAppCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString OpenAppCommand::getResult() const
{
    QString result = QString("OPEN_APP: %1\n[status]\n%2\npid=%3")
                         .arg(m_params.value("app").toString(),
                              m_started ? "Started" : "Failed",
                              QString::number(m_pid));
    if (!m_detachedLaunch) {
        result += QString("\nwaited=%1\nexitCode=%2")
                      .arg(m_finished ? "true" : "false")
                      .arg(m_exitCode);
    }
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}
