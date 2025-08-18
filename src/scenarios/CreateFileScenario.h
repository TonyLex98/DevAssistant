#pragma once
#include "IScenario.h"
#include <QRegularExpression>
#include <QDir>

class CreateFileScenario : public IScenario {
public:
    QString command() const override { return "CREATE_FILE"; }
    QString description() const override { return "создать новый файл с содержимым"; }
    QString dslHelp() const override {
        return "CREATE_FILE path=src/New.h\nCONTENT:\n<file content>\nEND\n";
    }
    QString parse(const QString& instruction, const QString& projectRoot, QVector<Change>& out) override;
};
