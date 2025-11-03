// CreateFileCommand.cpp
#include "CreateFileCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDateTime>

CreateFileCommand::CreateFileCommand(const QString& path,
                                     const QByteArray& content,
                                     const QString& descr)
{
    m_params["path"]    = path;
    m_params["content"] = QString::fromUtf8(content);
    m_description = descr.isEmpty()
        ? QString("Создать файл «%1»").arg(path)
        : descr;
}

bool CreateFileCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params["path"].toString();
    
    // Нормализуем путь: если путь относительный, делаем его относительно корневой директории проекта
    if (devAss && !QFileInfo(path).isAbsolute()) {
        path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
    }

    // Создаем родительскую директорию, если она не существует
    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (err) *err = QString("Не удалось создать директорию для файла: %1").arg(dir.path());
        return false;
    }

    // Проверяем, существует ли файл
    QFile f(path);
    if (f.exists()) {
        if (err) *err = QString("Файл уже существует: %1").arg(path);
        return false;
    }

    // Создаем файл
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QString("Не удалось создать файл: %1").arg(path);
        return false;
    }

    QByteArray content = m_params["content"].toString().toUtf8();
    qint64 bytesWritten = f.write(content);
    f.close();

    if (bytesWritten != content.size()) {
        if (err) *err = QString("Ошибка записи в файл: %1 (записано %2 байт из %3)")
                       .arg(path).arg(bytesWritten).arg(content.size());
        return false;
    }

    /* обновляем запись о файле в .dapro (если надо) */
    if (devAss) {
        devAss->updateFileDescription(path,
                                      QDateTime::currentDateTime(),
                                      true);
        devAss->save();
    }
    return true;
}

QJsonObject CreateFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString CreateFileCommand::getResult() const
{
    QString result;
    if (wasSuccessful()) {
        result = QString("CREATE_FILE: %1\n[status]\nFile created successfully\nexitCode=0")
               .arg(m_params["path"].toString());
    } else {
        result = QString("CREATE_FILE: %1\n[status]\nFile creation failed\nexitCode=1")
               .arg(m_params["path"].toString());
    }
    
    // Add error message if command failed
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}
