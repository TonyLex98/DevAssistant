#include "core/workflow/CommandRunner.h"
#include <QMap>
#include <QPointer>
#include <QThreadPool>
#include "core/commands/Command.h"
#include "core/commands/EditInRangeCommand.h"
#include "core/project/DevAssManager.h"
#include "core/util/json_utils.h"

CommandRunner::CommandRunner(DevAssManager& files, QObject* parent)
    : QObject(parent)
    , m_files(files)
{
}

void CommandRunner::run(const QList<CommandPtr>& commands,
                        std::function<void(Command*)> afterEach,
                        std::function<void(const Outcome&)> done)
{
    ++m_generation;
    m_queue.clear();
    m_edits.clear();
    for (const CommandPtr& c : commands) {
        if (c->name() == QLatin1String("EDIT_IN_RANGE"))
            m_edits << c;
        else
            m_queue << c;
    }
    m_afterEach = std::move(afterEach);
    m_done = std::move(done);
    m_outcome = Outcome();
    m_running = true;
    runNext();
}

void CommandRunner::cancel()
{
    ++m_generation;
    m_running = false;
    m_queue.clear();
    m_edits.clear();
    m_afterEach = nullptr;
    m_done = nullptr;
}

void CommandRunner::runNext()
{
    if (!m_running)
        return;
    if (m_queue.isEmpty()) {
        applyEditsAndFinish();
        return;
    }
    const CommandPtr command = m_queue.takeFirst();
    if (!command->runsInBackground()) {
        QString error;
        const bool ok = command->apply(&m_files, &error);
        finished(command, ok, error);
        return;
    }

    // The worker keeps the command alive (shared pointer) even if the batch is cancelled.
    const int generation = m_generation;
    QPointer<CommandRunner> self(this);
    DevAssManager* files = &m_files;
    QThreadPool::globalInstance()->start([self, command, files, generation]() {
        QString error;
        const bool ok = command->apply(files, &error);
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, command, ok, error, generation]() {
            if (self && generation == self->m_generation)
                self->finished(command, ok, error);
        }, Qt::QueuedConnection);
    });
}

void CommandRunner::finished(const CommandPtr& command, bool ok, const QString& error)
{
    command->setExecutionResult(ok, ok ? QString() : error);
    m_outcome.executed << command;
    const int generation = m_generation;
    if (m_afterEach)
        m_afterEach(command.get());
    if (generation != m_generation)
        return;  // cancelled from afterEach
    if (!ok) {
        m_outcome.failed = true;
        m_outcome.error = error.isEmpty() ? QStringLiteral("%1 failed").arg(command->name()) : error;
        m_queue.clear();
        m_edits.clear();
        applyEditsAndFinish();
        return;
    }
    runNext();
}

void CommandRunner::applyEditsAndFinish()
{
    // One batch per file, in the order the model gave them.
    QMap<QString, QList<EditInRangeCommand*>> byFile;
    QMap<QString, QList<CommandPtr>> owners;
    const QString root = m_files.projectRoot().absolutePath();
    for (const CommandPtr& c : m_edits) {
        auto* edit = static_cast<EditInRangeCommand*>(c.get());
        const QString path = normalizeContextPath(edit->path(), root);
        byFile[path] << edit;
        owners[path] << c;
    }
    m_edits.clear();
    for (auto it = byFile.begin(); it != byFile.end(); ++it) {
        QString error;
        const bool ok = EditInRangeCommand::applyBatch(it.value(), &m_files, &error);
        for (const CommandPtr& c : owners.value(it.key())) {
            c->setExecutionResult(ok, ok ? QString() : error);
            m_outcome.executed << c;
            if (m_afterEach)
                m_afterEach(c.get());
        }
        if (!ok) {
            m_outcome.failed = true;
            m_outcome.error = error;
            break;
        }
    }

    m_running = false;
    auto done = std::move(m_done);
    m_done = nullptr;
    m_afterEach = nullptr;
    if (done)
        done(m_outcome);
}
