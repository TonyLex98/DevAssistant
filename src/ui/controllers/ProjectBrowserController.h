#pragma once

#include <QObject>
#include <QString>

class QListWidget;
class MemoryLineEdit;
class StyledButton;
class ProjectIndex;
class DevAssManager;
class IDEWidget;

/*
 * Handles project root browsing, file scanning, and file list display.
 */
class ProjectBrowserController : public QObject
{
    Q_OBJECT

public:
    explicit ProjectBrowserController(QObject* parent = nullptr);

    // UI setup — returns a widget containing path edit + browse button
    QWidget* createPathWidget();
    StyledButton* rescanButton() const { return m_rescanBtn; }
    StyledButton* browseButton() const { return m_browseBtn; }

    // Dependencies
    void setProjectRoot(const QString& root);
    void setIndex(ProjectIndex* idx);
    void setDevManager(DevAssManager* mgr);
    void setIDEWidget(IDEWidget* ide);
    void setFileList(QListWidget* list);

    // Accessors
    QString projectRoot() const { return m_projectRoot; }
    QListWidget* fileList() const { return m_fileList; }
    MemoryLineEdit* rootEdit() const { return m_rootEdit; }

public slots:
    void onBrowse();
    void onRescan();

signals:
    void projectRootChanged(const QString& root);
    void filesListChanged();

private:
    void populateFileList();

    QString         m_projectRoot;
    QListWidget*    m_fileList;
    MemoryLineEdit* m_rootEdit;
    StyledButton*   m_browseBtn;
    StyledButton*   m_rescanBtn;

    ProjectIndex*   m_index;
    DevAssManager*  m_devManager;
    IDEWidget*      m_ideWidget;
};
