// PromptSequenceManager.h
#pragma once
#include <QObject>
#include <QMap>
#include <QString>
#include "PromptSequence.h"
#include <QDirIterator>

class PromptSequenceManager : public QObject
{
    Q_OBJECT
public:
    explicit PromptSequenceManager(QObject *parent = nullptr);

    /* Папка, где находятся последовательности */
    void setRoot(const QString& rootPath);          // <project>/DevelopersAssistant/PromptSequences
    QString root() const { return m_root; }

    /* Сканирование всех *.seq.json в папке и подпапках */
    void refresh();

    /* Доступ к уже загруженным объектам */
    const QMap<QString, PromptSequence>& sequences(); // key = full path

    /* Сохранить/удалить отдельную последовательность */
    bool save(const PromptSequence& seq, const QString& filePath);
    bool remove(const QString& filePath);

    /* Получить список файлов (полные пути) – удобно для UI‑выбора */
    QStringList fileList() const { return m_fileList; }

private:
    QString m_root;                     // директория PromptSequences
    QMap<QString, PromptSequence> m_sequences; // key = абсолютный путь к файлу
    QStringList m_fileList;
};
