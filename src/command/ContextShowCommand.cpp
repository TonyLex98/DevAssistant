// ContextShowCommand.cpp
#include "ContextShowCommand.h"

ContextShowCommand::ContextShowCommand(int maxChars, const QString& descr)
{
    m_params["maxChars"] = maxChars;
    m_description = descr.isEmpty()
        ? QStringLiteral("Shows current context content. Handled by UI. "
                         "Example: { \"command\": \"CONTEXT_SHOW\", \"params\": { \"maxChars\": 8000 } }")
        : descr;
}

bool ContextShowCommand::apply(DevAssManager* devAss, QString* err)
{
    Q_UNUSED(devAss)
    Q_UNUSED(err)
    return true;
}

QJsonObject ContextShowCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString ContextShowCommand::getResult() const
{
    const bool ok = wasSuccessful();
    QString result = QString("CONTEXT_SHOW\n[status]\n%1\nexitCode=%2")
                         .arg(ok ? "OK" : "FAILED")
                         .arg(ok ? 0 : 1);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

