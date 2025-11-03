// WriteDescriptionCommand.cpp
#include "WriteDescriptionCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

WriteDescriptionCommand::WriteDescriptionCommand(const QString& sourceFilePath,
                                                const QString& descr)
    : m_description(descr)
{
    m_params["sourceFilePath"] = sourceFilePath;
    m_params["description"]    = descr;
}

bool WriteDescriptionCommand::apply(DevAssManager* devAss, QString* err)
{
    if (!devAss) return false;

    QString srcPath = m_params["sourceFilePath"].toString();
    
    // Нормализуем путь: если путь относительный, делаем его относительно корневой директории проекта
    if (!QFileInfo(srcPath).isAbsolute()) {
        srcPath = QDir(devAss->projectRoot().absolutePath()).filePath(srcPath);
    }
    
    const QString descr  = m_params["description"].toString();

    // Получаем относительный путь относительно корня проекта
    QString rel = QDir(devAss->projectRoot().absolutePath())
                      .relativeFilePath(srcPath);

    QString descrPath = devAss->devAssDir().filePath(rel + ".dadescr");

    // создаём необходимые папки
    QDir().mkpath(QFileInfo(descrPath).absolutePath());

    QFile f(descrPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QString("Не удалось записать дескриптор %1").arg(descrPath);
        return false;
    }
    f.write(descr.toUtf8());
    f.close();

    /* обновляем статус в .dapro */
    devAss->updateFileDescription(srcPath,
                                  QDateTime::currentDateTime(),
                                  true);
    devAss->save();

    return true;
}

QJsonObject WriteDescriptionCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}
