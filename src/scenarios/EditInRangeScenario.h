#pragma once
#include "IScenario.h"
#include <QRegularExpression>
#include <QDir>
#include <QFile>

class EditInRangeScenario : public IScenario {
public:
    QString command() const override { return "EDIT_IN_RANGE"; }
    QString description() const override { return "заменить строки [from..to] в файле"; }
    QString dslHelp() const override {
        return "EDIT_IN_RANGE path=src/a.cpp from=3 to=7\nCONTENT:\n<replacement lines>\nEND\n";
    }
    QString parse(const QString& instruction, const QString& projectRoot, QVector<Change>& out) override;
};
