#include "core/context/ContextKeywords.h"
#include <QHash>
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <algorithm>

namespace ContextKeywords {

QSet<QString> extract(const QString& text)
{
    static const QSet<QString> stopwords = {
        // RU
        "и","в","во","на","по","к","ко","с","со","а","но","или","что","это","как","для","про","у","из","от","до","же","то","ли","не","да",
        // EN
        "the","a","an","and","or","to","of","in","on","for","with","is","are","was","were","be","been","as","at","by","from","that","this","it"
    };

    const QString lower = text.toLower();
    QRegularExpression re(QStringLiteral("[\\p{L}\\p{N}_]{3,}"));
    QRegularExpressionMatchIterator it = re.globalMatch(lower);

    QHash<QString, int> freq;
    while (it.hasNext()) {
        const QString token = it.next().captured(0);
        if (stopwords.contains(token))
            continue;
        freq[token]++;
    }

    QList<QPair<QString, int>> items;
    items.reserve(freq.size());
    for (auto f = freq.cbegin(); f != freq.cend(); ++f)
        items.push_back({f.key(), f.value()});

    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second)
            return a.second > b.second;
        return a.first < b.first;
    });

    QSet<QString> out;
    const int limit = std::min(30, items.size());
    for (int i = 0; i < limit; ++i)
        out.insert(items[i].first);
    return out;
}

double jaccard(const QSet<QString>& a, const QSet<QString>& b)
{
    if (a.isEmpty() || b.isEmpty())
        return 0.0;

    int inter = 0;
    for (const auto& k : a) {
        if (b.contains(k))
            ++inter;
    }
    const int uni = a.size() + b.size() - inter;
    return uni > 0 ? static_cast<double>(inter) / static_cast<double>(uni) : 0.0;
}

} // namespace ContextKeywords
