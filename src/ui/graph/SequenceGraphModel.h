#pragma once
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>
#include "core/workflow/PromptSequence.h"

class SolutionTemplate;
class SolutionTemplateManager;

// A sequence shown in the graph. Links in template.templ.json name sequences by file
// base name, while the graph shows sequence names, so both are kept together.
struct SequenceGraphEntry {
    QString baseName;
    PromptSequence sequence;
};

// A directed link between two sequences, by sequence name.
struct SequenceGraphLink {
    QString from;
    QString to;
};

// Decides which sequences and links the graph shows, and edits the links and the sequence
// list stored in template.templ.json. Nothing here knows about graphics items, so the view
// only turns the results into nodes and arrows.
namespace SequenceGraphModel {

// Sequences of the sequence manager, in the manager's order (by file path).
QVector<SequenceGraphEntry> entriesFromSequences(const QMap<QString, PromptSequence> &sequences);

// Every readable *.seq.json in the template folder, in directory listing order.
QVector<SequenceGraphEntry> entriesFromTemplateDir(const QString &templateDirPath);

// Links among the manager's sequences. They come from the sequenceTargets of the template file
// next to the first sequence, or from legacy OPEN_SEQUENCE markers when that file has none.
QVector<SequenceGraphLink> linksForSequences(const QMap<QString, PromptSequence> &sequences);

// Links among the sequences of one template folder; references to sequences outside it are dropped.
QVector<SequenceGraphLink> linksForTemplateDir(const QString &templateDirPath,
                                               const QVector<SequenceGraphEntry> &entries);

// File base name of a sequence that lives directly in templateDirPath, looked up among the
// manager's sequences. Returns false when none matches.
bool findBaseNameInSequences(const QMap<QString, PromptSequence> &sequences, const QString &sequenceName,
                             const QString &templateDirPath, QString *baseName);

// The same lookup done by reading the sequence files of the folder.
bool findBaseNameInDir(const QString &templateDirPath, const QString &sequenceName, QString *baseName);

// Path of <baseName>.seq.json in the folder; empty when baseName is empty.
QString sequenceFilePath(const QString &templateDirPath, const QString &baseName);

// Reads template.templ.json of the folder. Returns false for an empty path or an unreadable file.
bool loadTemplate(const QString &templateDirPath, SolutionTemplate &tmpl);

// Saves through the template manager so its cache stays current; writes the file directly
// only when there is no manager or it refuses.
bool saveTemplate(const SolutionTemplate &tmpl, const QString &templateDirPath,
                  SolutionTemplateManager *templateMgr);

// Adds the link sourceBase -> targetBase unless the template already has it.
void addLink(SolutionTemplate &tmpl, const QString &sourceBase, const QString &targetBase);

// Removes the link sourceBase -> targetBase. Returns false when sourceBase has no links at all.
bool removeLink(SolutionTemplate &tmpl, const QString &sourceBase, const QString &targetBase);

// Drops a sequence from the template: from the sequence list, the main sequence,
// every link to or from it, and the saved positions. The file itself is left alone.
void removeSequence(SolutionTemplate &tmpl, const QString &sequenceName, const QString &baseName,
                    const QString &templateDirPath);

} // namespace SequenceGraphModel
