#pragma once
#include <QString>
#include <QList>
#include <QByteArray>

QList<QByteArray> splitConcatenatedJson_fromText(const QString& text);

// Paths in the context: normalized against the project root, shown relative to it.
QString normalizeContextPath(const QString& path, const QString& projectRoot);
QString displayContextPath(const QString& normalizedPath, const QString& projectRoot);
