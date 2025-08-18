#include "ScenarioRegistry.h"
#include "CreateFileScenario.h"
#include "ReplaceInFileScenario.h"
#include "RenameFileScenario.h"
#include "DeleteFileScenario.h"
#include "EditInRangeScenario.h"

QVector<IScenario*> ScenarioRegistry::all(){
    QVector<IScenario*> v;
    v << new CreateFileScenario();
    v << new ReplaceInFileScenario();
    v << new RenameFileScenario();
    v << new DeleteFileScenario();
    v << new EditInRangeScenario();
    return v;
}

QString ScenarioRegistry::capabilitiesText(){
    QString out = "Доступные команды DSL:\n";
    auto v = all();
    for (IScenario* s : v) {
        out += QString("- %1 — %2\nПример:\n%3\n").arg(s->command(), s->description(), s->dslHelp());
        delete s;
    }
    return out;
}
