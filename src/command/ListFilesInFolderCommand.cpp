#include "ListFilesInFolderCommand.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDebug>

ListFilesInFolderCommand::ListFilesInFolderCommand(const QString& folderPath,
                                                   const QString& descr)
{
    m_params["folderPath"] = folderPath;
    m_description = descr.isEmpty()
        ? QString("Открыть список файлов в папке «%1»").arg(folderPath)
        : descr;
}

bool ListFilesInFolderCommand::apply(DevAssManager* devAss, QString* err)
{
    QString folderPath = m_params["folderPath"].toString();
    
    // Нормализуем путь: если путь относительный, делаем его относительно корневой директории проекта
    if (devAss && !QFileInfo(folderPath).isAbsolute()) {
        folderPath = QDir(devAss->projectRoot().absolutePath()).filePath(folderPath);
        // Обновляем путь в параметрах для отображения в результатах
        m_params["folderPath"] = folderPath;
    }

    // Проверяем, существует ли папка
    QDir dir(folderPath);
    if (!dir.exists()) {
        if (err) *err = QString("Папка не существует: %1").arg(folderPath);
        return false;
    }

    // Получаем список файлов в папке (только файлы, без рекурсии)
    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files, QDir::Name);
    
    if (fileInfoList.isEmpty()) {
        if (err) *err = QString("Папка не содержит файлов: %1").arg(folderPath);
        return false;
    }

    // Подсчитываем количество строк в каждом файле
    QStringList fileResults;
    for (const QFileInfo& fileInfo : fileInfoList) {
        QString filePath = fileInfo.absoluteFilePath();
        QFile file(filePath);
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            int lineCount = 0;
            while (!stream.atEnd()) {
                stream.readLine();
                ++lineCount;
            }
            
            fileResults << QString("%1 (%2 строк)").arg(fileInfo.fileName()).arg(lineCount);
            file.close();
        } else {
            fileResults << QString("%1 (не удалось открыть)").arg(fileInfo.fileName());
        }
    }

    // Сохраняем результат
    m_result = fileResults.join("\n");

    return true;
}

QJsonObject ListFilesInFolderCommand::toJson() const
{
    QJsonObject obj;
    obj["command"] = name();
    obj["params"] = m_params;
    obj["description"] = description();
    return obj;
}

QString ListFilesInFolderCommand::getResult() const
{
    QString result;
    if (wasSuccessful()) {
        result = QString("LIST_FILES_IN_FOLDER: %1\n[status]\nFile listing completed\nexitCode=0\n[files]\n%2")
               .arg(m_params["folderPath"].toString()).arg(m_result);
    } else {
        result = QString("LIST_FILES_IN_FOLDER: %1\n[status]\nFile listing failed\nexitCode=1")
               .arg(m_params["folderPath"].toString());
    }
    
    // Add error message if command failed
    if (!getErrorMessage().isEmpty()) {
        result += QString("\n[error]\n%1").arg(getErrorMessage());
    }
    
    return result;
}