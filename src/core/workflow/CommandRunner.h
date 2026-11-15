#pragma once
#include <QList>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class Command;
class DevAssManager;

using CommandPtr = std::shared_ptr<Command>;

// Runs a batch of commands one after another without blocking the window: commands that wait
// on a process or the network run in a worker thread (Command::runsInBackground()), the rest
// here. EDIT_IN_RANGE commands are applied last, together per file, so their line numbers all
// refer to the file as the model saw it. The first failure stops the batch.
class CommandRunner : public QObject
{
    Q_OBJECT
public:
    explicit CommandRunner(DevAssManager& files, QObject* parent = nullptr);

    struct Outcome
    {
        bool failed = false;
        QString error;
        QList<CommandPtr> executed;  // in the order they ran, the failed one included
    };

    // `afterEach` runs after every command that ran (successfully or not); `done` at the end.
    void run(const QList<CommandPtr>& commands,
             std::function<void(Command*)> afterEach,
             std::function<void(const Outcome&)> done);
    // Stops after the command that is running; `done` is not called.
    void cancel();
    bool isRunning() const { return m_running; }

private:
    void runNext();
    void finished(const CommandPtr& command, bool ok, const QString& error);
    void applyEditsAndFinish();

    DevAssManager& m_files;
    QList<CommandPtr> m_queue;
    QList<CommandPtr> m_edits;
    std::function<void(Command*)> m_afterEach;
    std::function<void(const Outcome&)> m_done;
    Outcome m_outcome;
    bool m_running = false;
    int m_generation = 0;  // results of a cancelled batch are ignored
};
