#include "core/session/Session.h"
#include <QDir>
#include <QSettings>
#include "core/util/AppConfig.h"

Session::Session(QObject* parent)
    : QObject(parent)
{
    connect(&m_project, &Project::opened, this, [this](const QString& root) {
        m_context.setProject(root, &m_project.manager());
    });
}

void Session::applyModelSettings(const AppConfig& config)
{
    const LlmClient::Provider provider = LlmClient::providerFromString(config.provider);
    QUrl endpoint = config.endpoint;
    if (!endpoint.isValid() || endpoint.toString().trimmed().isEmpty())
        endpoint = LlmClient::defaultEndpoint(provider);

    m_model.setProvider(provider);
    m_model.setEndpoint(endpoint);
    m_model.setApiKey(QSettings().value(QStringLiteral("model/apiKey")).toString());
    m_model.configure(config.selectedModel.trimmed(), config.generationParams());
}

void Session::loadWorkflows(const QString& baseDir)
{
    m_workflows.setFolders(QDir(baseDir).filePath(QStringLiteral("PromptSequences")),
                           QDir(baseDir).filePath(QStringLiteral("SolutionTemplates")));
}
