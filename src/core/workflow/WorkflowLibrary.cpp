#include "core/workflow/WorkflowLibrary.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include "core/util/Logging.h"
#include "core/workflow/BuiltinWorkflows.h"

namespace {

// The names a model may use for "foo.seq.json": foo, foo.seq, foo.seq.json, foo.json.
QStringList fileNamesFor(const QString& name)
{
    QStringList out{name};
    if (!name.endsWith(QLatin1String(".seq.json")))
        out << name + QStringLiteral(".seq.json");
    if (!name.endsWith(QLatin1String(".json")))
        out << name + QStringLiteral(".json");
    if (!name.endsWith(QLatin1String(".seq")))
        out << name + QStringLiteral(".seq");
    return out;
}

} // namespace

WorkflowLibrary::WorkflowLibrary()
{
    m_templates.setSequenceManager(&m_sequences);
}

void WorkflowLibrary::setFolders(const QString& sequencesDir, const QString& templatesDir)
{
    QDir().mkpath(sequencesDir);
    QDir().mkpath(templatesDir);
    m_sequences.setRoot(sequencesDir);
    m_templates.setRoot(templatesDir);

    const QString defaultFile = QDir(sequencesDir).filePath(QStringLiteral("default.seq.json"));
    if (!QFile::exists(defaultFile) && !m_sequences.save(BuiltinWorkflows::defaultSequence(), defaultFile))
        qCWarning(logSeq) << "Could not create" << defaultFile;
    if (!BuiltinWorkflows::installUniversalAgent(m_templates))
        qCWarning(logSeq) << "Could not install the Universal Agent template in" << templatesDir;
    refresh();
}

void WorkflowLibrary::refresh()
{
    m_templates.refresh();
    m_sequences.refresh();
    const QStringList known = m_sequences.fileList();
    for (const QString& dir : m_templates.templateDirs()) {
        const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("*.seq.json")},
                                                            QDir::Files | QDir::NoSymLinks, QDir::Name);
        for (const QFileInfo& file : files) {
            const QString path = file.absoluteFilePath();
            if (known.contains(path))
                continue;
            const Found f = load(path);
            if (f.isValid())
                m_sequences.addSequenceToCache(path, f.sequence);
        }
    }
}

WorkflowLibrary::Found WorkflowLibrary::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};
    Found f{PromptSequence::fromJson(doc.object()), path};
    if (f.sequence.name().isEmpty())
        return {};
    return f;
}

WorkflowLibrary::Found WorkflowLibrary::find(const QString& name, const QString& nearPath)
{
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
        return {};
    refresh();

    const QStringList fileNames = fileNamesFor(wanted);
    const QMap<QString, PromptSequence>& all = m_sequences.sequences();

    // The running sequence's folder first: a template's sequences refer to each other.
    if (!nearPath.isEmpty()) {
        const QDir dir = QFileInfo(nearPath).absoluteDir();
        for (const QString& fileName : fileNames) {
            const Found f = load(dir.filePath(fileName));
            if (f.isValid())
                return f;
        }
        for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
            if (QFileInfo(it.key()).absoluteDir() == dir && it.value().name() == wanted)
                return {it.value(), it.key()};
        }
    }
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
        if (it.value().name() == wanted)
            return {it.value(), it.key()};
    }
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
        const QFileInfo info(it.key());
        if (fileNames.contains(info.fileName()) || info.baseName() == wanted)
            return {it.value(), it.key()};
    }
    return {};
}

WorkflowLibrary::Found WorkflowLibrary::mainSequenceOf(const QString& templatePath, QString* error)
{
    m_templates.refresh();
    const QMap<QString, SolutionTemplate>& all = m_templates.templates();
    if (!all.contains(templatePath)) {
        if (error)
            *error = QCoreApplication::translate("WorkflowLibrary", "Template not found: %1").arg(templatePath);
        return {};
    }
    const SolutionTemplate tmpl = all.value(templatePath);
    const QDir dir(templatePath);
    QStringList candidates;
    if (!tmpl.mainSequencePath().isEmpty())
        candidates << dir.absoluteFilePath(tmpl.mainSequencePath());
    if (!tmpl.sequences().isEmpty())
        candidates << dir.absoluteFilePath(tmpl.sequences().first());
    for (const QString& path : candidates) {
        const Found f = load(path);
        if (f.isValid())
            return f;
    }
    if (error)
        *error = QCoreApplication::translate("WorkflowLibrary", "The template \"%1\" has no sequence that can be loaded.")
                         .arg(tmpl.name());
    return {};
}
