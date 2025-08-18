#pragma once
#include "IScenario.h"
#include <QRegularExpression>
#include <QDir>

class DeleteFileScenario : public IScenario {
public:
    QString command() const override { return "DELETE_FILE"; }
    QString description() const override { return "удалить файл"; }
    QString dslHelp() const override {
        return "DELETE_FILE path=assets/logo.jpg\n";
    }
    QString parse(const QString& instruction, const QString& projectRoot, QVector<Change>& out) override;
};
