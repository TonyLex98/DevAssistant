// UndoLastChangeCommand.cpp
#include "core/commands/UndoLastChangeCommand.h"

UndoLastChangeCommand::UndoLastChangeCommand(const QString& descr)
{
    m_description = descr.isEmpty()
        ? QStringLiteral("Undoes the last applied filesystem change batch (single level). "
                         "Example: { \"command\": \"UNDO_LAST_CHANGE\", \"params\": {} }")
        : descr;
}

bool UndoLastChangeCommand::apply(DevAssManager* devAss, QString* err)
{
    if (!devAss) {
        if (err) *err = "UNDO_LAST_CHANGE: DevAssManager is required";
        return false;
    }
    return devAss->undoLastChange(err);
}

QJsonObject UndoLastChangeCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString UndoLastChangeCommand::getResult() const
{
    const bool ok = wasSuccessful();
    QString result = QString("UNDO_LAST_CHANGE\n[status]\n%1\nexitCode=%2")
                         .arg(ok ? "Undo successful" : "Undo failed")
                         .arg(ok ? 0 : 1);
    if (!getErrorMessage().isEmpty())
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    return result;
}

