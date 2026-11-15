#include "core/project/Project.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMap>
#include "core/util/Logging.h"

Project::Project(QObject* parent)
    : QObject(parent)
{
}

void Project::open(const QString& folder)
{
    const QString root = QDir::cleanPath(QDir(folder).absolutePath());
    if (folder.trimmed().isEmpty() || !QDir(root).exists())
        return;
    m_root = root;
    m_manager.setProjectRoot(m_root);
    if (!m_manager.createFolder() || !m_manager.createDaproFile())
        qCWarning(logCore) << "Could not prepare the DevelopersAssistant folder in" << m_root;
    if (!m_manager.generateDescrFiles())
        qCWarning(logCore) << "Could not create the .dadescr files in" << m_root;
    emit opened(m_root);
    rescan();
}

void Project::rescan()
{
    if (!isOpen())
        return;
    m_manager.load();
    m_index.setRoot(m_root);
    emit scanned();
}

bool Project::isInAssistantFolder(const QString& absolutePath) const
{
    const QString assistantDir = m_manager.devAssDir().absolutePath();
    return !assistantDir.isEmpty() && absolutePath.startsWith(assistantDir);
}

QStringList Project::files() const
{
    QStringList out;
    const QDir root(m_root);
    for (const QString& rel : m_index.files()) {
        if (!isInAssistantFolder(root.filePath(rel)))
            out << QDir::fromNativeSeparators(rel);
    }
    return out;
}

QSet<QString> Project::filesNeedingDescription()
{
    QSet<QString> out;
    const QDir root(m_root);
    for (const QString& rel : files()) {
        const QString abs = root.filePath(rel);
        const FileDescription* fd = m_manager.findFileDescription(abs);
        if (!fd || QFileInfo(abs).lastModified() > fd->lastModTime)
            out.insert(abs);
    }
    return out;
}

QString Project::fileListText() const
{
    QStringList lines;
    const QDir root(m_root);
    for (const QString& rel : files())
        lines << QDir::toNativeSeparators(root.absoluteFilePath(rel));
    return lines.join(QLatin1Char('\n'));
}

QString Project::structureText() const
{
    if (!isOpen())
        return QStringLiteral("Cannot get the project structure: no project folder is open");
    const QStringList list = files();
    if (list.isEmpty())
        return QStringLiteral("The project is empty or contains no files");

    // Every folder that holds a file, with its parents, grouped by depth.
    QSet<QString> folders;
    for (const QString& file : list) {
        const QStringList parts = QFileInfo(file).path().split(QLatin1Char('/'));
        QString path;
        for (const QString& part : parts) {
            path = path.isEmpty() ? part : path + QLatin1Char('/') + part;
            folders.insert(path);
        }
    }
    QMap<int, QStringList> byDepth;
    for (const QString& folder : folders) {
        const int depth = (folder.isEmpty() || folder == QLatin1String(".")) ? 0 : folder.count(QLatin1Char('/')) + 1;
        byDepth[depth].append(folder);
    }

    QStringList out{QStringLiteral("Project structure (folders only):"), QString()};
    for (auto it = byDepth.begin(); it != byDepth.end(); ++it) {
        QStringList names = it.value();
        names.sort();
        for (const QString& folder : names) {
            QString name = folder.section(QLatin1Char('/'), -1);
            if (it.key() == 0)
                name = QFileInfo(m_root).fileName();
            out << QString(it.key() * 2, QLatin1Char(' ')) + name + QLatin1Char('/');
        }
    }
    return out.join(QLatin1Char('\n'));
}

QString Project::descriptionsText() const
{
    const QDir assistantDir = m_manager.devAssDir();
    if (!assistantDir.exists())
        return QString();
    QString out;
    QDirIterator it(assistantDir.absolutePath(), QStringList{QStringLiteral("*.dadescr")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString descrPath = it.next();
        QFile f(descrPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QString rel = assistantDir.relativeFilePath(descrPath);
        rel.chop(QStringLiteral(".dadescr").size());
        out += QStringLiteral("==== ") + QDir::toNativeSeparators(QDir(m_root).filePath(rel))
               + QStringLiteral(" ====\n") + QString::fromUtf8(f.readAll()).trimmed() + QStringLiteral("\n\n");
    }
    return out.trimmed();
}
