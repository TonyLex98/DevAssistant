#include "ProjectIndex.h"
#include <QDirIterator>

void ProjectIndex::setRoot(const QString& path) {
    m_root = path;
    rescan();
}

void ProjectIndex::rescan() {
    m_files.clear();
    if (m_root.isEmpty()) return;
    QDirIterator it(m_root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString abs = it.next();
        QString rel = QDir(m_root).relativeFilePath(abs);
        m_files << rel;
    }
    m_files.sort();
}
