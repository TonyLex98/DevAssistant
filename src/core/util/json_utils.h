#pragma once
#include <QString>
#include <QList>
#include <QByteArray>

QList<QByteArray> splitConcatenatedJson_fromText(const QString& text);

// Path normalization helpers (shared between MainWindow and controllers)
QString normalizeContextPath(const QString& path, const QString& projectRoot);
QString displayContextPath(const QString& normalizedPath, const QString& projectRoot);
