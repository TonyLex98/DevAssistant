// RenameFileCommand.cpp
#include "RenameFileCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

RenameFileCommand::RenameFileCommand(const QString& oldPath,
                                     const QString& newPath,
                                     const QString& descr)
{
    m_params["oldPath"] = oldPath;
    m_params["newPath"] = newPath;
    m_description = descr.isEmpty()
        ? QString("Переименовать файл из «%1» в «%2»")
              .arg(oldPath).arg(newPath)
        : descr;
}

bool RenameFileCommand::apply(DevAssManager* devAss, QString* err)
{
    QString oldPath = m_params["oldPath"].toString();
    QString newPath = m_params["newPath"].toString();

    // Нормализуем пути: если пути относительные, делаем их относительно корневой директории проекта
    if (devAss) {
        if (!QFileInfo(oldPath).isAbsolute()) {
            oldPath = QDir(devAss->projectRoot().absolutePath()).filePath(oldPath);
        }
        if (!QFileInfo(newPath).isAbsolute()) {
            newPath = QDir(devAss->projectRoot().absolutePath()).filePath(newPath);
        }
        // Обновляем пути в параметрах для отображения в результатах
        m_params["oldPath"] = oldPath;
        m_params["newPath"] = newPath;
    }

    // Создаем родительские директории для нового пути, если они не существуют
    QDir newDir = QFileInfo(newPath).absoluteDir();
    if (!newDir.exists() && !newDir.mkpath(".")) {
        if (err) *err = QString("Не удалось создать директорию для нового файла: %1").arg(newDir.path());
        return false;
    }

    if (!QFile::rename(oldPath, newPath)) {
        if (err) *err = QString("Не удалось переименовать %1 в %2")
                        .arg(oldPath).arg(newPath);
        return false;
    }

    /* обновляем запись в .dapro, если есть */
    if (devAss) {
        devAss->updateFileDescription(newPath,
                                      QDateTime::currentDateTime(),
                                      true);
        devAss->save();
    }
    return true;
}

QJsonObject RenameFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}
