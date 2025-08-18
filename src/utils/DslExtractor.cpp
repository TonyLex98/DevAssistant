#include "DslExtractor.h"
#include <QRegularExpression>
#include "../scenarios/ScenarioRegistry.h"

QString DslExtractor::extractAll(const QString& llmOut, const QString& projectRoot, QVector<Change>& out){
    out.clear();
    QString text = llmOut;
    text.remove(QRegularExpression("^```[a-zA-Z0-9]*", QRegularExpression::MultilineOption));
    text.remove(QRegularExpression("^```$", QRegularExpression::MultilineOption));

    auto scenarios = ScenarioRegistry::all();
    QString anyErr;
    for (IScenario* s : scenarios) {
        QString err = s->parse(text, projectRoot, out);
        if (!err.isEmpty()) anyErr = err;
        delete s;
    }
    if (out.isEmpty()) {
        QStringList blocks = text.split(QRegularExpression(R"((\r?\n){2,})"));
        for (const QString& b : blocks) {
            auto s2 = ScenarioRegistry::all();
            for (IScenario* s : s2) { QString err = s->parse(b, projectRoot, out); delete s; }
        }
    }
    if (out.isEmpty() && !anyErr.isEmpty()) return anyErr;
    return QString();
}
