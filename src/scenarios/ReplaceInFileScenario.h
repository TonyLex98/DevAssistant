#pragma once
#include "IScenario.h"
#include <QRegularExpression>
#include <QDir>

class ReplaceInFileScenario : public IScenario {
public:
    QString command() const override { return "REPLACE_IN_FILE"; }
    QString description() const override { return "заменить содержимое файла целиком/патчем"; }
    QString dslHelp() const override {
        return "REPLACE_IN_FILE path=src/a.cpp\nCONTENT:\n<new content>\nEND\n";
    }
    QString parse(const QString& instruction, const QString& projectRoot, QVector<Change>& out) override;
};
