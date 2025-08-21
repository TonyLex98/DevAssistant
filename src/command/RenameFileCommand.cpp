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
