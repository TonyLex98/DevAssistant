#pragma once
#include <QObject>
#include <QStringList>
#include "core/context/WorkingContext.h"
#include "core/model/LlmClient.h"
#include "core/project/Project.h"
#include "core/session/Conversation.h"
#include "core/workflow/PromptSequence.h"
#include "core/workflow/WorkflowLibrary.h"

class AppConfig;

// The sequence being run: which one, which step, and what the steps did so far.
struct RunState
{
    PromptSequence sequence;
    QString sequencePath;        // the file it was loaded from
    QString templatePath;        // the template it belongs to, if any
    QString contextSequenceId;   // its entry in the command history
    int stepIndex = -1;
    QStringList completedSteps;  // "Step 2: CREATE_FILE path=..." for {{PREVIOUS_STEPS}}

    bool isActive() const { return stepIndex >= 0; }
    const PromptStep* currentStep() const
    {
        return stepIndex >= 0 && stepIndex < sequence.steps().size() ? &sequence.steps()[stepIndex] : nullptr;
    }
};

// Everything the assistant works with, without a user interface: the project, the
// conversation, the working context, the sequences and templates, the models and the run.
// The window shows it; the agent engine changes it.
class Session : public QObject
{
    Q_OBJECT
public:
    explicit Session(QObject* parent = nullptr);

    Project& project() { return m_project; }
    const Project& project() const { return m_project; }
    Conversation& conversation() { return m_conversation; }
    const Conversation& conversation() const { return m_conversation; }
    WorkingContext& context() { return m_context; }
    WorkflowLibrary& workflows() { return m_workflows; }
    // The selected model; the chat, the sequences and agent mode all use it.
    LlmClient& model() { return m_model; }
    RunState& run() { return m_run; }
    const RunState& run() const { return m_run; }

    // Server, model and generation options from the settings.
    void applyModelSettings(const AppConfig& config);
    // Sequences in <documents>/DevelopersAssistant/PromptSequences, templates next to them.
    void loadWorkflows(const QString& baseDir);

private:
    Project m_project;
    Conversation m_conversation;
    WorkingContext m_context;
    WorkflowLibrary m_workflows;
    LlmClient m_model;
    RunState m_run;
};
