#pragma once
#include <QVector>
#include "IScenario.h"

class ScenarioRegistry {
public:
    static QVector<IScenario*> all();
    static QString capabilitiesText();
};
