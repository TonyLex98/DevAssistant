#pragma once
#include <QString>
#include <QVector>
#include "../core/ChangeApplier.h"

class DslExtractor {
public:
    static QString extractAll(const QString& llmOut, const QString& projectRoot, QVector<Change>& out);
};
