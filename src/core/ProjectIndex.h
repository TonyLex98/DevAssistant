#pragma once
#include <QString>
#include <QStringList>

class ProjectIndex {
public:
    void setRoot(const QString& path);
    QString root() const { return m_root; }
    QStringList files() const { return m_files; }
    void rescan();

private:
    QString m_root;
    QStringList m_files;
};
