// CreateFileCommand.cpp
#include "CreateFileCommand.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

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
    QByteArray cont = m_params["content"].toString().toUtf8();

    QFile f(path);
    if (f.exists() && !f.remove()) {
        if (err) *err = "Файл уже существует и не удалось удалить его";
        return false;
    }

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = "Не удалось создать файл";
        return false;
    }
    f.write(cont);
    f.close();

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
