#include "core/web/TextSummary.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <algorithm>

namespace Web {

namespace {

// The lower-cased words of `source`: runs of letters and digits.
QStringList tokenizeWords(const QString& source)
{
    static const QRegularExpression wordPattern(QStringLiteral("\\b[\\p{L}0-9]+\\b"),
                                                QRegularExpression::UseUnicodePropertiesOption);
    QStringList tokens;
    auto matchIt = wordPattern.globalMatch(source.toLower());
    while (matchIt.hasNext())
        tokens << matchIt.next().captured(0);
    return tokens;
}

// Splits after '.', '!' and '?'. Pieces shorter than 5 characters, such as list numbers, are
// dropped; the text after the last mark is kept.
QStringList splitIntoSentences(const QString& text)
{
    QStringList sentences;
    QString buffer;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (c == QLatin1Char('\r'))
            continue;
        buffer.append(c);
        if (c == '.' || c == '!' || c == '?') {
            QString trimmed = buffer.trimmed();
            if (trimmed.size() >= 5)
                sentences << trimmed;
            buffer.clear();
        }
    }
    if (!buffer.trimmed().isEmpty())
        sentences << buffer.trimmed();
    return sentences;
}

} // namespace

QString summarizeContent(const QString& content, const QString& query)
{
    const QString trimmedContent = content.trimmed();
    if (trimmedContent.isEmpty())
        return QStringLiteral("There is no text to summarize.");

    QStringList sentences = splitIntoSentences(trimmedContent);
    if (sentences.isEmpty())
        return trimmedContent.left(1000);

    QStringList queryTerms = tokenizeWords(query);
    queryTerms.removeDuplicates();

    struct SentenceScore {
        QString sentence;
        double score;
        int index;
    };

    QVector<SentenceScore> scored;
    scored.reserve(sentences.size());

    for (int index = 0; index < sentences.size(); ++index) {
        const QString& sentence = sentences[index];
        QStringList words = tokenizeWords(sentence);
        if (words.isEmpty())
            continue;

        QHash<QString, int> frequency;
        frequency.reserve(words.size());
        for (const QString& word : words)
            frequency[word]++;

        double score = 0.0;
        if (!queryTerms.isEmpty()) {
            for (const QString& term : queryTerms) {
                auto it = frequency.find(term);
                if (it != frequency.end())
                    score += it.value() * 2.5;
            }
            score += std::min(words.size(), 40) * 0.08;
        } else {
            score = words.size();
        }

        scored.append({sentence, score, index});
    }

    if (scored.isEmpty())
        return sentences.first();

    std::sort(scored.begin(), scored.end(), [](const SentenceScore& a, const SentenceScore& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.index < b.index;
    });

    const int maxSentences = std::min(3, static_cast<int>(scored.size()));
    QVector<SentenceScore> summarySentences;
    summarySentences.reserve(maxSentences);
    for (int i = 0; i < maxSentences; ++i)
        summarySentences.append(scored[i]);

    std::sort(summarySentences.begin(), summarySentences.end(), [](const SentenceScore& a, const SentenceScore& b) {
        return a.index < b.index;
    });

    QStringList summaryParts;
    for (const auto& info : summarySentences)
        summaryParts << info.sentence.trimmed();

    QString summary = summaryParts.join(" ");
    if (summary.isEmpty())
        summary = sentences.first();

    const QString effectiveQuery = query.trimmed().isEmpty() ? QStringLiteral("search query") : query.trimmed();
    return QString("Summary for the query '%1':\n%2").arg(effectiveQuery, summary.trimmed());
}

} // namespace Web
