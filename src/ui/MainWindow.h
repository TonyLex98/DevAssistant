#pragma once
#include <QDateTime>
#include <QMainWindow>
#include <QPointer>
#include <memory>
#include "core/project/DescriptionGenerator.h"
#include "core/session/Session.h"
#include "core/workflow/AgentEngine.h"
#include "ui/ChatPanel.h"
#include "ui/controllers/DialogAgentHost.h"
#include "ui/controllers/UIWidgets.h"

class LightRagWidget;
class SequenceEditorController;
class TaskStatusController;
class TtsController;
namespace Rag { class RagWidget; }

// The main window. It owns the session (everything the assistant works with) and the agent
// engine, builds the widgets (UiBuilder) and connects them: requests from the chat go to the
// engine, the engine's answers, results and log come back to the widgets.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Opens a project folder, as choosing it in the window does.
    void openProject(const QString& path);
    // Opens a project file (absolute, or relative to the project) in the editor.
    void openFileInEditor(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void connectProject();
    void connectChat();
    void connectEngine();
    void connectSettings();
    void connectTools();
    void restoreLayout();

    void handleRequest(const ChatPanel::Request& request);
    // Lets the user choose a sequence or template, then runs it.
    void chooseAndRun(bool templatesOnly);
    void runTemplate(const QString& templatePath);
    void showQuestion(const QString& text);
    void showResult(const QString& title, const QString& content, AgentEngine::ResultKind kind);
    void showProjectFiles();
    void showContext();
    void applyModelSettings();
    void describeFiles();
    void undoLastChange();
    void browse();
    void openDocuments();
    void openLightRag();
    void log(const QString& line);
    void showModelInStatusBar();

    Session m_session;
    DialogAgentHost m_host;
    AgentEngine m_engine;
    DescriptionGenerator m_describer;
    UIWidgets m_ui;
    TaskStatusController* m_status = nullptr;
    TtsController* m_tts = nullptr;
    std::unique_ptr<SequenceEditorController> m_sequences;
    QDateTime m_lastQuestionAt;
    QPointer<LightRagWidget> m_lightRag;   // delete themselves on close
    QPointer<Rag::RagWidget> m_documents;
};
