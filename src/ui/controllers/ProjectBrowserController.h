#pragma once
#include <QObject>
#include "../../services/IService.h"

class ProjectBrowserController : public QObject {
    Q_OBJECT
public:
    ProjectBrowserController(IProjectService* project, IContextService* ctx, IUIUpdater* ui);

    void browse();
    void rescan();
    void onFileChanged(const QString& path);
    void reOpenRootDir();

    QString buildProjectContext() const;
    QString fileDescriptionsText() const;

private:
    IProjectService* m_project;
    IContextService* m_ctx;
    IUIUpdater* m_ui;
};
