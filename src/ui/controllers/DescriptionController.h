#pragma once
#include <QObject>
#include "../../services/IService.h"

class DescriptionController : public QObject {
    Q_OBJECT
public:
    DescriptionController(IModelService* model, IProjectService* project, IUIUpdater* ui);

    void generateDescriptions();

private:
    IModelService* m_model;
    IProjectService* m_project;
    IUIUpdater* m_ui;
};
