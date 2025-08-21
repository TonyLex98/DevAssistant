// EditInRangeCommand.h
#pragma once
#include "Command.h"
#include <QStringList>

class EditInRangeCommand : public Command
{
public:
    EditInRangeCommand(const QString& path,
                       int startLine, int endLine,
                       const QByteArray& content,
                       const QString& descr = QString());

    QString name() const override { return "EDIT_IN_RANGE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;

private:
    QJsonObject m_params;
    QString m_description;
};
