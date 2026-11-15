// ContextClearCommand.cpp
#include "core/commands/ContextClearCommand.h"

ContextClearCommand::ContextClearCommand(const QString& scope, const QString& descr)
{
    m_params["scope"] = scope.isEmpty() ? QStringLiteral("active") : scope;
    m_description = descr.isEmpty()
        ? QStringLiteral("Clears context caches. scope: active|all. Handled by UI. "
                         "Example: { \"command\": \"CONTEXT_CLEAR\", \"params\": { \"scope\": \"active\" } }")
        : descr;
}

bool ContextClearCommand::apply(DevAssManager* devAss, QString* err)
{
    Q_UNUSED(devAss)
    Q_UNUSED(err)
    return true;
}

QJsonObject ContextClearCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString ContextClearCommand::getResult() const
{
    const bool ok = wasSuccessful();
    QString result = QString("CONTEXT_CLEAR\n[status]\n%1\nexitCode=%2")
                         .arg(ok ? "OK" : "FAILED")
                         .arg(ok ? 0 : 1);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

