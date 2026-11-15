#pragma once
#include <QString>
#include "core/workflow/PromptSequence.h"
#include "core/workflow/PromptSequenceManager.h"
#include "core/workflow/SolutionTemplateManager.h"

// The prompt sequences (*.seq.json) and solution templates on disk, and how to find them.
class WorkflowLibrary
{
public:
    WorkflowLibrary();

    // Sets the folders (created if needed), adds the built-in sequence and template, and loads all.
    void setFolders(const QString& sequencesDir, const QString& templatesDir);
    // Reads the folders again: plain sequences and the sequences inside every template.
    void refresh();

    PromptSequenceManager& sequences() { return m_sequences; }
    SolutionTemplateManager& templates() { return m_templates; }

    // A sequence found on disk.
    struct Found
    {
        PromptSequence sequence;
        QString path;
        bool isValid() const { return !path.isEmpty(); }
    };
    // Finds a sequence the model asked for: by its name, by its file name with or without
    // ".seq.json", first among the loaded sequences, then in the folder of `nearPath`
    // (the sequence that is running, so a template's own sequences are found).
    Found find(const QString& name, const QString& nearPath);
    // The sequence a template starts with: its main sequence, or else its first one.
    Found mainSequenceOf(const QString& templatePath, QString* error);

    static Found load(const QString& path);

private:
    PromptSequenceManager m_sequences;
    SolutionTemplateManager m_templates;
};
