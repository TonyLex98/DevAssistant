// DeleteFileCommand.cpp
#include "DeleteFileCommand.h"
#include <QFile>
#include <QDebug>

DeleteFileCommand::DeleteFileCommand(const QString& path,
                                     const QString& descr)
{
    m_params["path"] = path;
    m_description = descr.isEmpty()
        ? QString("Удалить файл «%1»").arg(path)
        : descr;
}

bool DeleteFileCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params["path"].toString();
    
    // Нормализуем путь: если путь относительный, делаем его относительно корневой директории проекта
    if (devAss && !QFileInfo(path).isAbsolute()) {
        path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
        // Обновляем путь в параметрах для отображения в результатах
        m_params["path"] = path;
    }

    if (!QFile::remove(path)) {
        if (err) *err = QString("Не удалось удалить файл %1").arg(path);
        return false;
    }

    /* удалить запись в .dapro, если есть */
    if (devAss) {
        devAss->findFileDescription(path);   // нет прямого remove; можно оставить как есть
        devAss->save(); // можно оставить без изменений
    }
    return true;
}

QJsonObject DeleteFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString DeleteFileCommand::getResult() const
{
    QString result;
    if (wasSuccessful()) {
        result = QString("DELETE_FILE: %1\n[status]\nFile deleted successfully\nexitCode=0")
               .arg(m_params["path"].toString());
    } else {
        result = QString("DELETE_FILE: %1\n[status]\nFile deletion failed\nexitCode=1")
               .arg(m_params["path"].toString());
    }
    
    // Add error message if command failed
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}
