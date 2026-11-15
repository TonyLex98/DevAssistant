#pragma once
#include <QCoreApplication>
#include <QPointer>
#include <QString>
#include "core/workflow/PromptSequence.h"

class QWidget;
class SequenceGraphWidget;
class Session;

// Creating and editing prompt sequences, and the sequence graph: which template it shows and
// which sequence is running.
class SequenceEditorController
{
    Q_DECLARE_TR_FUNCTIONS(SequenceEditorController)
public:
    SequenceEditorController(Session& session, SequenceGraphWidget* graph, QWidget* dialogParent);

    // Reloads the sequences and templates and redraws the graph.
    void refreshGraph();
    // The template chosen in the chat; the graph shows it.
    void setSelectedTemplate(const QString& templatePath);
    // Highlights the running sequence.
    void highlightRunningSequence();

    void openEditorFor(const QString& sequenceName);
    // Lets the user pick a sequence file and opens it in the editor.
    void editSequences();
    void createNewSequence();
    // The solution templates in a window.
    void showTemplates();
    // Makes a template from one of the sequences.
    void exportSequenceAsTemplate();

private:
    void openEditorWindow(const QString& title, const QString& filePath, const PromptSequence& seq);
    void redrawGraph();

    Session& m_session;
    QPointer<SequenceGraphWidget> m_graph;
    QWidget* m_dialogParent;
    QString m_templatePath;
};
