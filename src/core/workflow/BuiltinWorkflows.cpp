#include "core/workflow/BuiltinWorkflows.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include "core/util/Logging.h"
#include "core/workflow/SolutionTemplate.h"
#include "core/workflow/SolutionTemplateManager.h"

namespace BuiltinWorkflows {

PromptSequence defaultSequence()
{
    PromptStep clarify;
    clarify.setTemplate(
        "You are an AI assistant for refactoring and editing code.\n"
        "Project context:\n{{PROJECT_ROOT}}\n"
        "Files:\n{{FILE_LIST}}\n"
        "\n{{HISTORY}}\n"
        "If the user's task is not entirely clear, ask up to 3 clarifying questions with the ASK_USER command (one JSON object per question).\n"
        "If needed, call SEARCH_PROJECT (search the project) or SEARCH_WEB (external sources) before writing the plan.\n"
        "Available commands: {{SUPPORTED_COMMANDS}}\n"
        "Then describe a short plan of action.");
    clarify.setAllowedCommands({"ASK_USER", "OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB"});

    PromptStep gather;
    gather.setTemplate(
        "Based on the request and the plan, pick the smallest pieces of code you need.\n"
        "Use OPEN_FILE_REGION (preferably with startLine/endLine or a regular expression) for specific fragments.\n"
        "If you need a whole file (rarely), use OPEN_FILE.\n"
        "For full-text search in the project, use SEARCH_PROJECT (set maxMatches and, if needed, include).\n"
        "If you need outside information, call SEARCH_WEB and briefly summarize what you found.\n"
        "Output only commands.");
    gather.setAllowedCommands({"OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB"});

    PromptStep draft;
    draft.setTemplate(
        "Prepare the set of edits.\n"
        "• To replace text: REPLACE_IN_FILE\n"
        "• To edit a range of lines: EDIT_IN_RANGE\n"
        "• To create a file: CREATE_FILE\n"
        "• To rename or delete: RENAME_FILE/DELETE_FILE\n"
        "Output only the list of JSON commands, without explanations.\n"
        "Context:\n{{OPEN_FILES}}\n");
    draft.setAllowedCommands({"REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE"});

    PromptStep questions;
    questions.setTemplate(
        "If 1-2 details are missing to make the edits precise, ask about them with ASK_USER; otherwise return an empty list.");
    questions.setAllowedCommands({"ASK_USER"});

    PromptStep finish;
    finish.setTemplate(
        "Output the final commands that apply the changes (repeat the whole set, without explanations).\n"
        "If the user should see a result or instructions, add a SHOW_RESULT command with title/content.");
    finish.setAllowedCommands({"REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE", "SHOW_RESULT"});

    PromptSequence seq(QStringLiteral("Default"));
    seq.setDescription(QStringLiteral(
        "Step-by-step editing: clarify → collect minimal context → draft edits → follow-up questions → final commands."));
    for (const PromptStep& step : {clarify, gather, draft, questions, finish})
        seq.addStep(step);
    return seq;
}

namespace {

const QStringList& allCommands()
{
    static const QStringList all = {
        "ASK_USER", "OPEN_FILE", "OPEN_FILE_REGION",
        "SEARCH_PROJECT", "SEARCH_PROJECT_CASE_INSENSITIVE", "SEARCH_WEB", "SEARCH_DOCS",
        "LIST_FILES_IN_FOLDER", "CONTEXT_SHOW", "CONTEXT_CLEAR",
        "CREATE_FILE", "REPLACE_IN_FILE", "EDIT_IN_RANGE", "INSERT_AT_LINE", "APPEND_TO_FILE",
        "RENAME_FILE", "COPY_FILE", "DELETE_FILE",
        "CREATE_DIR", "DELETE_DIR", "APPLY_PATCH",
        "RUN_SHELL", "RUN_PYTHON",
        "GIT_STATUS", "GIT_DIFF", "GIT_RESTORE_FILE", "UNDO_LAST_CHANGE",
        "WRITE_DESCRIPTION", "OPEN_SEQUENCE", "CALL_SEQUENCE", "REASONING",
        "SHOW_RESULT", "OPEN_APP", "OPEN_URL", "CLIPBOARD_SET", "CLIPBOARD_GET"
    };
    return all;
}

PromptStep step(const char* tmpl)
{
    PromptStep s;
    s.setTemplate(QString::fromUtf8(tmpl));
    s.setAllowedCommands(allCommands());
    return s;
}

PromptSequence sequence(const char* name, const char* description, std::initializer_list<PromptStep> steps)
{
    PromptSequence seq(QString::fromUtf8(name));
    seq.setDescription(QString::fromUtf8(description));
    for (const PromptStep& s : steps)
        seq.addStep(s);
    return seq;
}

bool writeSequence(const QString& path, const PromptSequence& seq)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logSeq) << "Could not write" << path;
        return false;
    }
    file.write(QJsonDocument(seq.toJson()).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

bool installUniversalAgent(SolutionTemplateManager& templates)
{
    // Bump when the sequences below change, so installed copies are rewritten.
    const QString version = QStringLiteral("4");
    const QString dir = QDir(templates.root()).filePath(QStringLiteral("UniversalAgent"));
    const QString versionFile = QDir(dir).filePath(QStringLiteral(".universal_agent.version"));
    if (!QDir().mkpath(dir))
        return false;
    {
        QFile f(versionFile);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text) && QString::fromUtf8(f.readAll()).trimmed() == version)
            return true;
    }

    const QList<QPair<QString, PromptSequence>> sequences = {
        {"main.seq.json", sequence("Universal Agent Main",
            "Universal multi-stage flow: analysis -> context -> implementation -> verification.", {
            step("Current date: {{CURRENT_DATE}}\n"
                 "Current datetime: {{CURRENT_DATETIME}}\n\n"
                 "User request:\n{{USER_QUERY}}\n\n"
                 "Context:\n{{HISTORY}}\n{{CONTEXT_CHAIN}}\n\n"
                 "First, produce a short plan and identify missing context.\n"
                 "If context is missing, call SEARCH_PROJECT/OPEN_FILE_REGION/SEARCH_WEB.\n"
                 "If this is mostly informational/search task, produce SHOW_RESULT after collecting enough evidence.\n"
                 "If full implementation is needed, use OPEN_SEQUENCE with sequenceName from {{OPEN_SEQUENCE_INFO}}.\n"
                 "Do not repeat the same SEARCH_WEB query in a loop.\n"
                 "Return JSON commands only.\n\n"
                 "Allowed commands:\n{{SUPPORTED_COMMANDS}}"),
            step("Build an execution plan based on gathered context.\n"
                 "Project structure:\n{{PROJECT_STRUCTURE}}\n\n"
                 "Open files:\n{{OPEN_FILES}}\n\n"
                 "If the user request is already answerable, emit SHOW_RESULT now.\n"
                 "Otherwise decide the next focused sequence using OPEN_SEQUENCE when useful (analysis/coding/testing).\n"
                 "Never end this step with only repeated search commands.\n"
                 "Return JSON commands only.")})},
        {"file-analysis.seq.json", sequence("Universal Agent File Analysis",
            "Deep file-level and architecture analysis for the task.", {
            step("Analyze project files deeply for this request:\n{{USER_QUERY}}\n\n"
                 "Use FILE_LIST, PROJECT_STRUCTURE, OPEN_FILE_REGION and SEARCH_PROJECT to find exact implementation points.\n"
                 "Summarize hotspots and then OPEN_SEQUENCE to context-chain or code-generation.\n"
                 "If this request is informational and no code edits are needed, emit SHOW_RESULT.\n"
                 "Return JSON commands only.\n\n"
                 "{{FILE_LIST}}\n{{PROJECT_STRUCTURE}}\n{{OPEN_SEQUENCE_INFO}}")})},
        {"context-chain.seq.json", sequence("Universal Agent Context Chain",
            "Use executed command context to guide the next actions.", {
            step("Analyze context chain and previous results:\n{{CONTEXT_CHAIN}}\n\n"
                 "Detect outdated assumptions, missing checks, and next best action.\n"
                 "If enough evidence is collected and no more actions are needed, emit SHOW_RESULT.\n"
                 "If code changes are ready, OPEN_SEQUENCE to code-generation.\n"
                 "If verification is needed, OPEN_SEQUENCE to testing.\n"
                 "Return JSON commands only.")})},
        {"code-generation.seq.json", sequence("Universal Agent Code Generation",
            "Generate and refine code changes with minimal risk.", {
            step("Implement the requested change.\n"
                 "Request: {{USER_QUERY}}\n"
                 "Context files:\n{{OPEN_FILES}}\n\n"
                 "Prefer minimal diffs, preserve architecture, and avoid unrelated edits.\n"
                 "Create/edit files and run local checks as needed.\n"
                 "Return JSON commands only."),
            step("Review your produced changes for regressions and obvious defects.\n"
                 "Use GIT_DIFF/GIT_STATUS and targeted checks.\n"
                 "Then OPEN_SEQUENCE to testing if validation is needed.\n"
                 "If validation is already sufficient, emit SHOW_RESULT.\n"
                 "Return JSON commands only.")})},
        {"testing.seq.json", sequence("Universal Agent Testing",
            "Validate implementation and provide final status.", {
            step("Run verification for the implemented change.\n"
                 "Use available test/build/lint commands and summarize failures.\n"
                 "If failures require fixes, OPEN_SEQUENCE to code-generation.\n"
                 "If no runnable tests exist, still produce a concise SHOW_RESULT with manual verification notes.\n"
                 "Return JSON commands only."),
            step("Prepare final concise result for the user.\n"
                 "Include what changed, how validated, and residual risks.\n"
                 "Use SHOW_RESULT for final message (mandatory).\n"
                 "Return JSON commands only.")})},
    };

    SolutionTemplate tmpl(QStringLiteral("Universal Agent"));
    tmpl.setDescription(QStringLiteral("Universal multi-sequence agent for coding, search, editing and verification."));
    tmpl.setMainSequencePath(QStringLiteral("main.seq.json"));
    for (const auto& s : sequences) {
        if (!writeSequence(QDir(dir).filePath(s.first), s.second))
            return false;
        tmpl.addSequence(s.first);
    }
    tmpl.setTargetsForSequence(QStringLiteral("main"),
                               QJsonArray{"file-analysis", "context-chain", "code-generation", "testing"});
    tmpl.setTargetsForSequence(QStringLiteral("file-analysis"), QJsonArray{"context-chain", "code-generation"});
    tmpl.setTargetsForSequence(QStringLiteral("context-chain"), QJsonArray{"code-generation", "testing", "main"});
    tmpl.setTargetsForSequence(QStringLiteral("code-generation"), QJsonArray{"testing", "main"});
    tmpl.setTargetsForSequence(QStringLiteral("testing"), QJsonArray{"code-generation", "main"});
    if (!templates.save(tmpl, dir))
        return false;

    QFile f(versionFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(version.toUtf8());
    return true;
}

} // namespace BuiltinWorkflows
