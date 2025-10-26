// EditInRangeCommand.h
#pragma once
#include "Command.h"
#include <QStringList>
#include <QJsonArray>

class EditInRangeCommand : public Command
{
public:
    EditInRangeCommand(const QString& path,
                       int startLine, int endLine,
                       const QByteArray& content,
                       const QJsonArray& replacementLines = QJsonArray(),
                       const QString& descr = QString());

    QString name() const override { return "EDIT_IN_RANGE"; }
    QJsonObject params() const override { return m_params; }
    QString description() const override { return m_description; }
    bool apply(DevAssManager* devAss, QString* err) override;
    QJsonObject toJson() const override;

    const QString& path() const { return m_path; }
    int startLine() const { return m_startLine; }
    int endLine() const { return m_endLine; }
    const QStringList& newLines() const { return m_newLines; }

    static bool applyBatch(const QList<EditInRangeCommand*>& commands,
                           DevAssManager* devAss,
                           QString* err);

private:
    bool applyToLines(QStringList& lines, QString* err) const;
    static bool loadFileLines(const QString& path, QStringList& out, QString* err);
    static bool saveFileLines(const QString& path,
                              const QStringList& lines,
                              DevAssManager* devAss,
                              QString* err);

    QJsonObject m_params;
    QString m_description;
    QString m_path;
    int m_startLine = 0;
    int m_endLine = 0;
    QStringList m_newLines;
};
