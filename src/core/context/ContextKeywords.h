#pragma once

#include <QSet>
#include <QString>

// Keyword overlap is how the context tells whether two texts are about the same thing;
// both the archive and the context graph use it, so it lives apart from either.
namespace ContextKeywords {

// The 30 most frequent words of three or more letters, without common stopwords.
QSet<QString> extract(const QString& text);

// Jaccard index of the two sets; 0 when either is empty.
double jaccard(const QSet<QString>& a, const QSet<QString>& b);

} // namespace ContextKeywords
