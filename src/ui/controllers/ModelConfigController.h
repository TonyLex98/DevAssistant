#pragma once
#include <QObject>
#include "../../services/IService.h"

class ModelConfigController : public QObject {
    Q_OBJECT
public:
    ModelConfigController(IModelService* model, ISettingsService* settings, IUIUpdater* ui, IProjectService* project);

public slots:
    void openSettings();
    void editBasePrompt();
    void setupModelWithGenerationParams();
    void applySettingsFromDialog();

private:
    IModelService* m_model;
    ISettingsService* m_settings;
    IUIUpdater* m_ui;
    IProjectService* m_project;
};
