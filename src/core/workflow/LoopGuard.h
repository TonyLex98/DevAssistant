#pragma once
#include <QList>
#include <QString>

class Command;

// Stops a run that goes in circles: answers without usable commands, step after step of
// searching and reading without a result, the same commands again and again. Small local
// models do this; without a guard they would run until the user stops them.
class LoopGuard
{
public:
    void reset();

    // SHOW_RESULT was shown: the run has given its answer.
    void resultShown();
    bool hasShownResult() const { return m_resultShown; }

    // An answer with no command to run (none, or all rejected). True: give up.
    bool emptyBatch();

    // A batch that is about to run. Returns why the run should stop, or an empty string.
    QString check(const QList<Command*>& batch);

    // Commands that look around (search, read, think) without changing anything.
    static bool isExploration(const QList<Command*>& batch);
    // What the batch does, for spotting repeats: names and their main parameter.
    static QString signature(const QList<Command*>& batch);

private:
    bool m_resultShown = false;
    int m_batchesWithoutResult = 0;
    int m_sameBatchRepeats = 0;
    QString m_lastSignature;
};
