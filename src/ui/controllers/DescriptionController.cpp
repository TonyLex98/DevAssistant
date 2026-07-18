#include "DescriptionController.h"
#include <QDirIterator>
#include <QProgressDialog>
#include <QMessageBox>
#include <QFileInfo>
#include "DescriptionDialog.h"

DescriptionController::DescriptionController(IModelService* model, IProjectService* project, IUIUpdater* ui)
    : QObject(nullptr)
    , m_model(model)
    , m_project(project)
    , m_ui(ui)
{
}

void DescriptionController::generateDescriptions()
{
    DescriptionGenerator generator(m_model->modelPtr(), m_project->devManagerPtr(), nullptr);

    // 1. Перебираем файлы проекта
    QStringList fileList;
    QDirIterator it(m_project->projectRoot(), QDir::Files, QDirIterator::Subdirectories);
    const QString devAssPath = m_project->devAssDirPath();

    while (it.hasNext()) {
        const QString srcFilePath = it.next();
        if (srcFilePath.startsWith(devAssPath))
            continue;

        FileDescription* fd = m_project->findFileDescription(srcFilePath);
        QDateTime fileMod = QFileInfo(srcFilePath).lastModified();

        if (fd && fd->isDescriptionUpToDate && fileMod <= fd->lastModTime)
            continue;

        fileList << srcFilePath;
    }

    if (fileList.isEmpty()) {
        // No parent widget for QMessageBox::information — uses application-wide modal
        QMessageBox::information(nullptr, "Готово",
                                 "В проекте нет файлов, требующих описания.");
        return;
    }

    // 2. Генерация с прогресс-диалогом
    QProgressDialog prog("Генерация описаний…", "Отмена",
                         0, fileList.size(), nullptr);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);

    int processed = 0, failed = 0;
    for (const QString &srcFilePath : fileList) {
        if (prog.wasCanceled()) break;

        QString descr = generator.generateAndSave(srcFilePath);
        if (descr.isEmpty()){
            ++failed;
        }
        else{
            QString relPath = QDir(m_project->projectRoot()).relativeFilePath(srcFilePath);
            QString descrPath = m_project->devAssDirPath() + QDir::separator() + relPath + ".dadescr";
            QString descrFileName = QFileInfo(descrPath).fileName();
            m_ui->log(QString("✓ Описан %1 → %2").arg(relPath).arg(descrFileName));
        }
        ++processed;
        prog.setValue(processed);
    }

    prog.setValue(fileList.size());
    m_ui->log(QString("Генерация завершена: %1 успешно, %2 ошибок.")
              .arg(processed - failed).arg(failed));

    m_project->rescan();
}
