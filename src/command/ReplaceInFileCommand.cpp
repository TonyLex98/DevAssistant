// ReplaceInFileCommand.cpp
#include "ReplaceInFileCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

ReplaceInFileCommand::ReplaceInFileCommand(const QString& path,
                                          const QByteArray& content,
                                          const QString& descr)
{
    m_params["path"]    = path;
    m_params["content"] = QString::fromUtf8(content);
    m_description = descr.isEmpty()
        ? QString("Заменить содержимое файла «%1»").arg(path)
        : descr;
}

bool ReplaceInFileCommand::apply(DevAssManager* devAss, QString* err)
{
    QString path = m_params["path"].toString();
    
    // Нормализуем путь: если путь относительный, делаем его относительно корневой директории проекта
    if (devAss && !QFileInfo(path).isAbsolute()) {
        path = QDir(devAss->projectRoot().absolutePath()).filePath(path);
    }

    QByteArray cont = m_params["content"].toString().toUtf8();

    // Создаем родительские директории, если они не существуют
    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (err) *err = QString("Не удалось создать директорию для файла: %1").arg(dir.path());
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QString("Не удалось открыть файл для записи: %1").arg(path);
        return false;
    }
    f.write(cont);
    f.close();

    /* обновляем запись о файле в .dapro */
    if (devAss) {
        devAss->updateFileDescription(path,
                                      QDateTime::currentDateTime(),
                                      true);
        devAss->save();
    }
    return true;
}

QJsonObject ReplaceInFileCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString ReplaceInFileCommand::getResult() const
{
    QString result;
    if (wasSuccessful()) {
        result = QString("REPLACE_IN_FILE: %1\n[status]\nFile content replaced successfully\nexitCode=0")
               .arg(m_params["path"].toString());
    } else {
        result = QString("REPLACE_IN_FILE: %1\n[status]\nFile content replacement failed\nexitCode=1")
               .arg(m_params["path"].toString());
    }
    
    // Add error message if command failed
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}
