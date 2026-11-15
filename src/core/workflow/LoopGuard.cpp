#include "core/workflow/LoopGuard.h"
#include <QSet>
#include <QStringList>
#include "core/commands/Command.h"

namespace {
constexpr int kMaxEmptyBatches = 7;        // answers without a usable command
constexpr int kMaxExplorationBatches = 8;  // batches that only look around
constexpr int kMaxRepeats = 3;             // the same batch in a row
} // namespace

void LoopGuard::reset()
{
    *this = LoopGuard();
}

void LoopGuard::resultShown()
{
    m_resultShown = true;
    m_batchesWithoutResult = 0;
    m_sameBatchRepeats = 0;
    m_lastSignature.clear();
}

bool LoopGuard::emptyBatch()
{
    return ++m_batchesWithoutResult >= kMaxEmptyBatches && !m_resultShown;
}

QString LoopGuard::check(const QList<Command*>& batch)
{
    for (const Command* c : batch) {
        if (c && c->name() == QLatin1String("SHOW_RESULT")) {
            resultShown();
            return QString();
        }
    }
    ++m_batchesWithoutResult;
    const QString sig = signature(batch);
    if (!sig.isEmpty() && sig == m_lastSignature) {
        ++m_sameBatchRepeats;
    } else {
        m_lastSignature = sig;
        m_sameBatchRepeats = 1;
    }
    if (!isExploration(batch))
        return QString();
    if (m_batchesWithoutResult >= kMaxExplorationBatches)
        return QStringLiteral("too many steps without a final answer");
    if (m_sameBatchRepeats >= kMaxRepeats)
        return QStringLiteral("the same commands repeat without progress");
    return QString();
}

bool LoopGuard::isExploration(const QList<Command*>& batch)
{
    static const QSet<QString> exploring = {
        QStringLiteral("SEARCH_WEB"), QStringLiteral("SEARCH_PROJECT"), QStringLiteral("SEARCH_PROJECT_CASE_INSENSITIVE"),
        QStringLiteral("SEARCH_DOCS"), QStringLiteral("OPEN_FILE"), QStringLiteral("OPEN_FILE_REGION"),
        QStringLiteral("OPEN_SEQUENCE"), QStringLiteral("CALL_SEQUENCE"), QStringLiteral("LIST_FILES_IN_FOLDER"),
        QStringLiteral("CONTEXT_SHOW"), QStringLiteral("REASONING"), QStringLiteral("WRITE_DESCRIPTION"),
        QStringLiteral("CLIPBOARD_GET")
    };
    if (batch.isEmpty())
        return false;
    for (const Command* c : batch) {
        if (c && !exploring.contains(c->name()))
            return false;
    }
    return true;
}

QString LoopGuard::signature(const QList<Command*>& batch)
{
    static const char* const mainParams[] = {"query", "sequenceName", "path", "command", "script"};
    QStringList parts;
    for (const Command* c : batch) {
        if (!c)
            continue;
        const QJsonObject p = c->params();
        QString marker;
        for (const char* key : mainParams) {
            if (p.contains(QLatin1String(key))) {
                marker = p.value(QLatin1String(key)).toString().trimmed().left(120);
                break;
            }
        }
        parts << c->name() + QLatin1Char(':') + marker;
    }
    return parts.join(QLatin1Char('|'));
}
