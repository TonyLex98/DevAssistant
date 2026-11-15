#pragma once

#include <QString>
#include "core/workflow/PromptSequence.h"
#include "core/workflow/PromptSequenceManager.h"
#include "core/workflow/SolutionTemplateManager.h"
#include "ui/controllers/IService.h"

// Sequence editor controller:
//  - creates, edits and saves sequences
//  - drives the sequence graph
//  - refreshes the sequence cache from the sequences folder and the template folders
class SequenceEditorController
{
public:
    SequenceEditorController(ISequenceService* seq, ITemplateService* tmpl, IGraphService* graph, IUIUpdater* ui);

    void openSequenceGraph();

    // Highlights the running sequence in the graph.
    void updateSequenceGraphHighlight();

    void updateSequenceGraphTemplateSelection();

    void openSequenceEditorForName(const QString& sequenceName);

    // Lets the user pick a sequence file and opens it in the editor.
    void editSequences();

    void createNewSequence();

    // Reloads all sequences: the main folder plus the template folders.
    void refreshAllSequences();

    // Writes default.seq.json if it does not exist.
    void ensureDefaultSequence();

    void onChatTemplateSelectionChanged(const QString& templatePath);

    static PromptSequence defaultSequenceTemplate();

private:
    ISequenceService* m_seq;
    ITemplateService* m_tmpl;
    IGraphService* m_graph;
    IUIUpdater* m_ui;

    void openSequenceEditorWindow(const QString& title,
                                  const QString& filePath,
                                  const PromptSequence& seq);
};
