#include "ProjectBrowserController.h"
#include <QListWidget>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QWidget>
#include <QHBoxLayout>
#include "MemoryLineEdit.h"
#include "StyledButton.h"
#include "ProjectIndex.h"
#include "DevAssManager.h"
#include "IDEWidget.h"

ProjectBrowserController::ProjectBrowserController(QObject* parent)
    : QObject(parent)
    , m_fileList(nullptr)
    , m_rootEdit(nullptr)
    , m_browseBtn(nullptr)
    , m_rescanBtn(nullptr)
    , m_index(nullptr)
    , m_devManager(nullptr)
    , m_ideWidget(nullptr)
{
}

QWidget* ProjectBrowserController::createPathWidget()
{
    auto* widget = new QWidget();
    auto* lay = new QHBoxLayout(widget);
    lay->setContentsMargins(0, 0, 0, 0);

    m_rootEdit = new MemoryLineEdit("projectRoot", nullptr);
    m_browseBtn = new StyledButton("...", nullptr);
    m_rescanBtn = new StyledButton(tr("Обновить проект"), nullptr);

    lay->addWidget(m_rootEdit, 1);
    lay->addWidget(m_browseBtn);
    lay->addWidget(m_rescanBtn);

    connect(m_browseBtn, &StyledButton::clicked, this, &ProjectBrowserController::onBrowse);
    connect(m_rescanBtn, &StyledButton::clicked, this, &ProjectBrowserController::onRescan);

    return widget;
}

void ProjectBrowserController::setProjectRoot(const QString& root)
{
    m_projectRoot = root;
    if (m_rootEdit)
        m_rootEdit->setText(root);
    emit projectRootChanged(root);
}

void ProjectBrowserController::setIndex(ProjectIndex* idx)
{
    m_index = idx;
}

void ProjectBrowserController::setDevManager(DevAssManager* mgr)
{
    m_devManager = mgr;
}

void ProjectBrowserController::setIDEWidget(IDEWidget* ide)
{
    m_ideWidget = ide;
}

void ProjectBrowserController::setFileList(QListWidget* list)
{
    m_fileList = list;
}

void ProjectBrowserController::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(nullptr, tr("Выберите корень проекта"), m_projectRoot);
    if (!dir.isEmpty()) {
        setProjectRoot(dir);
        onRescan();
    }
}

void ProjectBrowserController::onRescan()
{
    if (!m_index || !m_fileList || m_projectRoot.isEmpty())
        return;

    // Load .dapro before scanning
    if (m_devManager)
        m_devManager->load();

    const QString devAssPath = m_devManager ? m_devManager->devAssDir().absolutePath() : QString();
    const QString projectRootPath = m_projectRoot;

    m_index->setRoot(m_projectRoot);
    m_fileList->clear();

    QSet<QString> needRedSet;

    for (const QString& rel : m_index->files()) {
        const QString absPath = QDir(projectRootPath).filePath(rel);

        // Filter out DevelopersAssistant files
        if (!devAssPath.isEmpty() && absPath.startsWith(devAssPath))
            continue;

        QListWidgetItem* item = new QListWidgetItem(QDir::fromNativeSeparators(rel), m_fileList);

        FileDescription* fd = nullptr;
        if (m_devManager)
            fd = m_devManager->findFileDescription(absPath);

        QDateTime fileMod = QFileInfo(absPath).lastModified();

        bool needRed = false;
        if (!fd) {
            needRed = true;
        } else if (fileMod > fd->lastModTime) {
            needRed = true;
        }

        if (needRed) {
            needRedSet.insert(absPath);
            item->setBackgroundColor(Qt::red);
        } else {
            item->setBackgroundColor(Qt::green);
        }

        item->setData(Qt::UserRole, absPath);
    }

    emit filesListChanged();
}
