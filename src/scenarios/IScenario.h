#pragma once
#include <QString>
#include <QVector>
#include "../core/ChangeApplier.h"

class IScenario {
public:
    virtual ~IScenario(){}
    virtual QString command() const = 0;
    virtual QString description() const = 0;
    virtual QString dslHelp() const = 0;
    virtual QString parse(const QString& instruction, const QString& projectRoot, QVector<Change>& out) = 0;
};
