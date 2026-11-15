#include "ui/graph/SequenceGraphModel.h"
#include "core/workflow/SolutionTemplate.h"
#include "core/workflow/SolutionTemplateManager.h"
#include "core/util/Logging.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace {

const char kTemplateFileName[] = "template.templ.json";
const char kSequenceSuffix[] = ".seq.json";
const char kSequenceFilter[] = "*.seq.json";

QString templateFilePath(const QString &templateDirPath)
{
    return QDir(templateDirPath).filePath(QString::fromLatin1(kTemplateFileName));
}

bool readTemplateObject(const QString &templateDirPath, QJsonObject *templateObj)
{
    const QString path = templateFilePath(templateDirPath);
    QFile file(path);
    if (!file.exists()) {
        qCDebug(logViewer) << "[SequenceGraphModel] Main template file does not exist:" << path;
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCDebug(logViewer) << "[SequenceGraphModel] Could not open template file:" << path;
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qCDebug(logViewer) << "[SequenceGraphModel] Invalid JSON in template file:" << path;
        return false;
    }
    *templateObj = doc.object();
    return true;
}

// Any failure means the caller falls back to the legacy markers, so it only needs a yes or no.
bool readSequenceTargets(const QString &templateDirPath, QJsonObject *sequenceTargets)
{
    QJsonObject templateObj;
    if (!readTemplateObject(templateDirPath, &templateObj))
        return false;
    if (!templateObj.contains("sequenceTargets")) {
        qCDebug(logViewer) << "[SequenceGraphModel] No sequenceTargets in template file, using legacy method";
        return false;
    }
    *sequenceTargets = templateObj.value("sequenceTargets").toObject();
    return true;
}

// When two files map to the same key, the later one wins, as in the directory listing.
QMap<QString, QString> baseNameToSequenceName(const QVector<SequenceGraphEntry> &entries)
{
    QMap<QString, QString> map;
    for (const SequenceGraphEntry &entry : entries) {
        map[entry.baseName] = entry.sequence.name();
        qCDebug(logViewer) << "[SequenceGraphModel] Mapped filename" << entry.baseName << "to sequence name" << entry.sequence.name();
    }
    return map;
}

QVector<SequenceGraphLink> linksFromTargets(const QJsonObject &sequenceTargets,
                                            const QMap<QString, QString> &baseNameToName)
{
    QVector<SequenceGraphLink> links;
    for (auto it = sequenceTargets.constBegin(); it != sequenceTargets.constEnd(); ++it) {
        const QString sourceBaseName = it.key();
        const QJsonArray targetArray = it.value().toArray();
        const QString sourceSequenceName = baseNameToName.value(sourceBaseName, sourceBaseName);

        for (const QJsonValue &targetValue : targetArray) {
            const QString targetBaseName = targetValue.toString();
            const QString targetSequenceName = baseNameToName.value(targetBaseName, targetBaseName);

            // A link to a file that is not part of the graph would point at nothing.
            if (baseNameToName.contains(sourceBaseName) && baseNameToName.contains(targetBaseName)) {
                links.append(SequenceGraphLink{sourceSequenceName, targetSequenceName});
                qCDebug(logViewer) << "[SequenceGraphModel] Found directed connection:" << sourceSequenceName << "->" << targetSequenceName;
            } else {
                qCDebug(logViewer) << "[SequenceGraphModel] Sequence not found for directed connection:" << sourceBaseName << "->" << targetBaseName;
            }
        }
    }
    return links;
}

// Templates without sequenceTargets linked sequences only through {{OPEN_SEQUENCE:name,args}}
// markers in step text. The marker may give the sequence name or the file base name.
QVector<SequenceGraphLink> legacyLinks(const QList<PromptSequence> &sources, const QSet<QString> &names,
                                       const QMap<QString, QString> &fileToName)
{
    const QRegularExpression marker(R"(\{\{OPEN_SEQUENCE:([^,}]+),?([^}]*)\}\})");
    QVector<SequenceGraphLink> links;
    for (const PromptSequence &seq : sources) {
        for (const PromptStep &step : seq.steps()) {
            QRegularExpressionMatchIterator iter = marker.globalMatch(step.tmpl());
            while (iter.hasNext()) {
                const QString reference = iter.next().captured(1).trimmed();

                QString target = reference;
                if (!names.contains(reference) && fileToName.contains(reference))
                    target = fileToName.value(reference);

                if (names.contains(target)) {
                    links.append(SequenceGraphLink{seq.name(), target});
                    qCDebug(logViewer) << "[SequenceGraphModel] Found legacy connection:" << seq.name() << "->" << target << "(from reference:" << reference << ")";
                } else {
                    qCDebug(logViewer) << "[SequenceGraphModel] Reference to a sequence outside the graph ignored:" << seq.name() << "->" << reference;
                }
            }
        }
    }
    return links;
}

// Every manager sequence is scanned, including ones that share a name.
QVector<SequenceGraphLink> legacyLinksAcrossSequences(const QVector<SequenceGraphEntry> &entries)
{
    QList<PromptSequence> sources;
    QSet<QString> names;
    for (const SequenceGraphEntry &entry : entries) {
        sources.append(entry.sequence);
        names.insert(entry.sequence.name());
    }
    return legacyLinks(sources, names, baseNameToSequenceName(entries));
}

// Sequences of a template are keyed by name: a duplicate name keeps the later file,
// and links are produced in name order.
QVector<SequenceGraphLink> legacyLinksWithinTemplate(const QVector<SequenceGraphEntry> &entries)
{
    QMap<QString, PromptSequence> byName;
    QSet<QString> names;
    for (const SequenceGraphEntry &entry : entries) {
        byName[entry.sequence.name()] = entry.sequence;
        names.insert(entry.sequence.name());
    }
    return legacyLinks(byName.values(), names, baseNameToSequenceName(entries));
}

} // namespace

namespace SequenceGraphModel {

QVector<SequenceGraphEntry> entriesFromSequences(const QMap<QString, PromptSequence> &sequences)
{
    QVector<SequenceGraphEntry> entries;
    entries.reserve(sequences.size());
    for (auto it = sequences.constBegin(); it != sequences.constEnd(); ++it)
        entries.append(SequenceGraphEntry{QFileInfo(it.key()).baseName(), it.value()});
    return entries;
}

QVector<SequenceGraphEntry> entriesFromTemplateDir(const QString &templateDirPath)
{
    QVector<SequenceGraphEntry> entries;
    const QStringList filters{QString::fromLatin1(kSequenceFilter)};
    const QFileInfoList fileInfoList = QDir(templateDirPath).entryInfoList(filters, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &fileInfo : fileInfoList) {
        const QString seqPath = fileInfo.absoluteFilePath();
        QFile seqFile(seqPath);
        if (!seqFile.open(QIODevice::ReadOnly)) {
            qCDebug(logViewer) << "[SequenceGraphModel] Could not open sequence file:" << seqPath;
            continue;
        }
        const QJsonDocument seqDoc = QJsonDocument::fromJson(seqFile.readAll());
        if (!seqDoc.isObject()) {
            qCDebug(logViewer) << "[SequenceGraphModel] Invalid JSON in sequence file:" << seqPath;
            continue;
        }
        entries.append(SequenceGraphEntry{fileInfo.baseName(), PromptSequence::fromJson(seqDoc.object())});
    }
    return entries;
}

QVector<SequenceGraphLink> linksForSequences(const QMap<QString, PromptSequence> &sequences)
{
    if (sequences.isEmpty())
        return {};
    const QString firstSeqPath = sequences.firstKey();
    if (firstSeqPath.isEmpty())
        return {};

    // The manager is pointed at one template folder, so the first sequence tells where its template file is.
    const QString templateDir = QFileInfo(firstSeqPath).absolutePath();
    const QVector<SequenceGraphEntry> entries = entriesFromSequences(sequences);

    QJsonObject sequenceTargets;
    if (!readSequenceTargets(templateDir, &sequenceTargets))
        return legacyLinksAcrossSequences(entries);
    return linksFromTargets(sequenceTargets, baseNameToSequenceName(entries));
}

QVector<SequenceGraphLink> linksForTemplateDir(const QString &templateDirPath,
                                               const QVector<SequenceGraphEntry> &entries)
{
    QJsonObject sequenceTargets;
    if (!readSequenceTargets(templateDirPath, &sequenceTargets))
        return legacyLinksWithinTemplate(entries);
    return linksFromTargets(sequenceTargets, baseNameToSequenceName(entries));
}

bool findBaseNameInSequences(const QMap<QString, PromptSequence> &sequences, const QString &sequenceName,
                             const QString &templateDirPath, QString *baseName)
{
    for (auto it = sequences.constBegin(); it != sequences.constEnd(); ++it) {
        if (it.value().name() != sequenceName)
            continue;
        const QFileInfo fi(it.key());
        if (fi.absolutePath() == templateDirPath) {
            *baseName = fi.baseName();
            return true;
        }
    }
    return false;
}

bool findBaseNameInDir(const QString &templateDirPath, const QString &sequenceName, QString *baseName)
{
    const QVector<SequenceGraphEntry> entries = entriesFromTemplateDir(templateDirPath);
    for (const SequenceGraphEntry &entry : entries) {
        if (entry.sequence.name() == sequenceName) {
            *baseName = entry.baseName;
            return true;
        }
    }
    return false;
}

QString sequenceFilePath(const QString &templateDirPath, const QString &baseName)
{
    if (baseName.isEmpty())
        return QString();
    return QDir(templateDirPath).filePath(baseName + QString::fromLatin1(kSequenceSuffix));
}

bool loadTemplate(const QString &templateDirPath, SolutionTemplate &tmpl)
{
    // An empty path would resolve the file name against the working directory.
    if (templateDirPath.isEmpty())
        return false;

    QJsonObject templateObj;
    if (!readTemplateObject(templateDirPath, &templateObj))
        return false;
    tmpl = SolutionTemplate::fromJson(templateObj);
    return true;
}

bool saveTemplate(const SolutionTemplate &tmpl, const QString &templateDirPath,
                  SolutionTemplateManager *templateMgr)
{
    if (templateMgr) {
        if (templateMgr->save(tmpl, templateDirPath))
            return true;
        qCDebug(logViewer) << "[SequenceGraphModel] TemplateManager save failed, fallback to manual write";
    }

    if (templateDirPath.isEmpty())
        return false;

    QFile file(templateFilePath(templateDirPath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(QJsonDocument(tmpl.toJson()).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void addLink(SolutionTemplate &tmpl, const QString &sourceBase, const QString &targetBase)
{
    QJsonObject targets = tmpl.getSequenceTargets();
    QJsonArray arr = targets.value(sourceBase).toArray();
    if (!arr.contains(targetBase))
        arr.append(targetBase);
    targets[sourceBase] = arr;
    tmpl.setSequenceTargets(targets);
}

bool removeLink(SolutionTemplate &tmpl, const QString &sourceBase, const QString &targetBase)
{
    QJsonObject targets = tmpl.getSequenceTargets();
    if (!targets.contains(sourceBase))
        return false;

    const QJsonArray arr = targets.value(sourceBase).toArray();
    QJsonArray updated;
    for (const QJsonValue &val : arr) {
        if (val.toString() != targetBase) {
            updated.append(val);
        }
    }

    // An empty list would leave a source with no links behind in the file.
    if (updated.isEmpty()) {
        targets.remove(sourceBase);
    } else {
        targets[sourceBase] = updated;
    }
    tmpl.setSequenceTargets(targets);
    return true;
}

void removeSequence(SolutionTemplate &tmpl, const QString &sequenceName, const QString &baseName,
                    const QString &templateDirPath)
{
    // The template may list the file by its name or by its full path.
    const QString fileName = baseName.isEmpty() ? QString() : baseName + QString::fromLatin1(kSequenceSuffix);
    const QString absPath = sequenceFilePath(templateDirPath, baseName);

    QVector<QString> updatedSeqs;
    for (const QString &seqPath : tmpl.sequences()) {
        if (seqPath == fileName || seqPath == absPath)
            continue;
        updatedSeqs.append(seqPath);
    }
    tmpl.clearSequences();
    for (const QString &s : updatedSeqs) {
        tmpl.addSequence(s);
    }

    if (tmpl.mainSequencePath() == fileName || tmpl.mainSequencePath() == absPath) {
        tmpl.setMainSequencePath(QString());
    }

    QJsonObject targets = tmpl.getSequenceTargets();
    if (!baseName.isEmpty()) {
        targets.remove(baseName);
        for (auto it = targets.begin(); it != targets.end();) {
            const QJsonArray arr = it.value().toArray();
            QJsonArray filtered;
            for (const QJsonValue &val : arr) {
                if (val.toString() != baseName) {
                    filtered.append(val);
                }
            }
            if (filtered.isEmpty()) {
                it = targets.erase(it);
            } else {
                it.value() = filtered;
                ++it;
            }
        }
    }
    tmpl.setSequenceTargets(targets);

    QMap<QString, QPointF> positions = tmpl.positions();
    positions.remove(sequenceName);
    tmpl.clearPositions();
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        tmpl.setPositionForSequence(it.key(), it.value());
    }
}

} // namespace SequenceGraphModel
