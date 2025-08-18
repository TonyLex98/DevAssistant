#pragma once
#include "IScenario.h"
#include <QRegularExpression>
#include <QDir>

class RenameFileScenario : public IScenario {
public:
    QString command() const override { return "RENAME_FILE"; }
    QString description() const override { return "переименовать файл"; }
    QString dslHelp() const override {
        return "RENAME_FILE from=old/name.h to=new/name.h\n";
    }
    QString parse(const QString& instruction, const QString& projectRoot, QVector<Change>& out) override;
};
