// PromptSequenceManager.cpp
#include "PromptSequenceManager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>

PromptSequenceManager::PromptSequenceManager(QObject *parent)
    : QObject(parent)
{
}

void PromptSequenceManager::setRoot(const QString &rootPath)
{
    m_root = rootPath;
    refresh();
}

void PromptSequenceManager::refresh()
{
    m_sequences.clear();
    m_fileList.clear();

    if (m_root.isEmpty())
        return;

    QDir dir(m_root);
    if (!dir.exists())
        return;

    // ищем *.seq.json во всех подпапках
    QStringList filters = {"*.seq.json"};
    QFileInfoList files = dir.entryInfoList(filters,
                                            QDir::Files | QDir::NoSymLinks,
                                            QDir::Name);

    // рекурсивный поиск
    QDirIterator it(m_root,
                    filters,
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;

        QByteArray data = f.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) continue;

        PromptSequence seq = PromptSequence::fromJson(doc.object());
        m_sequences.insert(path, seq);
        m_fileList << path;
    }

    // Sequences added from elsewhere (solution templates) survive a rescan of the folder.
    for (auto it = m_cachedExtra.constBegin(); it != m_cachedExtra.constEnd(); ++it) {
        if (!m_sequences.contains(it.key())) {
            m_sequences.insert(it.key(), it.value());
            m_fileList << it.key();
        }
    }
}

const QMap<QString, PromptSequence> &PromptSequenceManager::sequences()
{
    refresh();
    return m_sequences;
}

/* Сохранить/перезаписать */
bool PromptSequenceManager::save(const PromptSequence &seq, const QString &filePath)
{
    QJsonObject obj = seq.toJson();
    QJsonDocument doc(obj);
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();

    // обновляем кеш
    m_sequences.insert(filePath, seq);
    if (!m_fileList.contains(filePath))
        m_fileList << filePath;
    return true;
}

/* Удалить файл */
bool PromptSequenceManager::remove(const QString &filePath)
{
    if (!QFile::remove(filePath))
        return false;
    m_sequences.remove(filePath);
    m_fileList.removeAll(filePath);
    return true;
}

/* Добавить/обновить отдельную последовательность в кэш (для использования с шаблонами) */
void PromptSequenceManager::addSequenceToCache(const QString& filePath, const PromptSequence& sequence)
{
    m_cachedExtra.insert(filePath, sequence);
    m_sequences.insert(filePath, sequence);
    if (!m_fileList.contains(filePath))
        m_fileList << filePath;
}

/* Найти последовательность по имени (возвращается путь к файлу и сама последовательность) */
QPair<QString, PromptSequence> PromptSequenceManager::findSequenceByName(const QString& name) const
{
    for (auto it = m_sequences.constBegin(); it != m_sequences.constEnd(); ++it) {
        if (it.value().name() == name) {
            return qMakePair(it.key(), it.value()); // path, sequence
        }
    }
    return qMakePair(QString(), PromptSequence()); // Empty pair if not found
}
