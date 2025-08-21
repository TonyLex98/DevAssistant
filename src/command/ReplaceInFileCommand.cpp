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
    QByteArray cont = m_params["content"].toString().toUtf8();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = "Не удалось открыть файл для записи";
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
