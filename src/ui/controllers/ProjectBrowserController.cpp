#include "ProjectBrowserController.h"
#include <QFileDialog>
#include <QListWidget>
#include <QDirIterator>
#include <QSet>
#include "../../core/DevAssManager.h"
#include "utils/Logging.h"

ProjectBrowserController::ProjectBrowserController(IProjectService* project, IContextService* ctx, IUIUpdater* ui)
    : QObject(nullptr)
    , m_project(project)
    , m_ctx(ctx)
    , m_ui(ui)
{
}

void ProjectBrowserController::browse()
{
    QString d = QFileDialog::getExistingDirectory(nullptr, "Select project folder", m_project->projectRoot());
    if (d.isEmpty()) return;

    m_project->setProjectRoot(d);
    reOpenRootDir();
}

void ProjectBrowserController::rescan()
{
    m_project->loadDevManager();

    const QString devAssPath = m_project->devAssDirPath();
    const QString projectRootPath = m_project->projectRoot();

    m_project->setIndexRoot(projectRootPath);
    m_project->filesList()->clear();

    QSet<QString> needRed_set;

    for (const QString& rel : m_project->indexedFiles()) {
        const QString absPath = QDir(projectRootPath).filePath(rel);

        if (absPath.startsWith(devAssPath))
            continue;

        QListWidgetItem* item = new QListWidgetItem(QDir::fromNativeSeparators(rel), m_project->filesList());

        FileDescription* fd = m_project->findFileDescription(absPath);
        QDateTime fileMod = QFileInfo(absPath).lastModified();

        bool needRed = false;
        if (!fd) {
            needRed = true;
        } else if (fileMod > fd->lastModTime) {
            needRed = true;
        }

        if (needRed){
            needRed_set.insert(absPath);
            item->setBackgroundColor(Qt::red);
        }
        else
            item->setBackgroundColor(Qt::green);

        item->setData(Qt::UserRole, absPath);
    }

    m_project->emitMissingFilesUpdated(needRed_set);

    m_ui->log("Rescanned project.");
}

void ProjectBrowserController::onFileChanged(const QString& path)
{
    if (m_ctx->fileHasContextCommands(path)) {
        const QSet<QString>& commands = m_ctx->getContextCommandsForFile(path);
        if (!commands.isEmpty()) {
            m_ui->log(QString("File changed externally: %1. Context from commands %2 may be stale.")
                      .arg(path).arg(commands.values().join(", ")));
            m_ctx->clearContextForPath(path);
        }
    }
}

void ProjectBrowserController::reOpenRootDir()
{
    if (m_project->projectRoot().isEmpty()) {
        return;
    }

    m_ctx->fileWatcher()->removePaths(m_ctx->fileWatcher()->files());
    m_ctx->clearFileToContextCommands();
    m_ctx->clearShellCommandOutputs();
    m_ctx->clearPythonScriptOutputs();

    m_project->openProjectInIDE(m_project->projectRoot());

    m_project->setRootEditText(m_project->projectRoot());
    m_project->setDevProjectRoot(m_project->projectRoot());
    m_project->setPromptProjectRoot(m_project->projectRoot());

    auto* dm = m_project->devManagerPtr();
    if (!dm->createFolder() || !dm->createDaproFile())
    {
        qCritical(logProject) << "Не удалось подготовить DevelopersAssistant / .dapro";
    }

    if (!dm->generateDescrFiles())
    {
        qCritical(logProject) << "Ошибка при создании .dadescr файлов";
    }

    rescan();
}

QString ProjectBrowserController::buildProjectContext() const
{
    const QString projectRootPath = m_project->projectRoot();
    const QString devAssPath = m_project->devAssDirPath();

    QString s = "Корневая папка: " + projectRootPath + "\nФайлы:\n";

    int file_counter = 0;
    for (const QString& f : m_project->indexedFiles()) {
        const QString abs = QDir(projectRootPath).filePath(f);

        if (abs.startsWith(devAssPath) ||
            f.startsWith(QStringLiteral("DevelopersAssistant") + QDir::separator()) ||
            f.endsWith(QStringLiteral(".dadescr")))
            continue;

        s += " - " + QDir::toNativeSeparators(abs) + '\n';
        ++file_counter;

        const QString rel = QDir(projectRootPath).relativeFilePath(abs);
        const QString descrPath =
            m_project->devAssDirPath() + QDir::separator() + rel + ".dadescr";
        QFile descrFile(descrPath);
        if (descrFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString descr = QString::fromUtf8(descrFile.readAll());
            descrFile.close();

            if (!descr.isEmpty()) {
                s += "   Описание:\n";
                s += "   " + descr.replace("\n", "\n   ") + "\n";
            }
        }
    }

    if (file_counter == 0) {
        s += QStringLiteral("Файлов в проекте пока нет.\n");
    }

    if (!m_ctx->contextContent().isEmpty())
        s += "\nСодержимое файлов, добавленных в контекст:\n" + m_ctx->contextContent() + "\n";

    if (m_ctx->hasContextManager()) {
        QString contextChain = m_ctx->generateContextText();
        if (!contextChain.isEmpty()) {
            s += "\nЦепочка контекста из выполненных команд:\n" + contextChain + "\n";
        }
    }

    return s;
}

QString ProjectBrowserController::fileDescriptionsText() const
{
    QString out;
    QDir dev = m_project->devManagerPtr()->devAssDir();
    if (!dev.exists()) return out;

    QDirIterator it(dev.absolutePath(), QStringList() << "*.dadescr", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString descrPath = it.next();
        QFile f(descrPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QString descr = QString::fromUtf8(f.readAll());
        f.close();

        QString rel = dev.relativeFilePath(descrPath);
        rel.chop(QString(".dadescr").size());
        QString absTarget = QDir(m_project->projectRoot()).filePath(rel);

        out += "==== " + QDir::toNativeSeparators(absTarget) + " ====\n";
        out += descr.trimmed() + "\n\n";
    }
    return out.trimmed();
}
