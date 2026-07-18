#include "ModelConfigController.h"
#include "../OllamaSettingsDialog.h"
#include "../PromptEditorDialog.h"
#include "../utils/AppConfig.h"
#include "../../adapters/OllamaAdapter.h"
#include <QJsonArray>

ModelConfigController::ModelConfigController(IModelService* model, ISettingsService* settings, IUIUpdater* ui, IProjectService* project)
    : QObject(nullptr)
    , m_model(model)
    , m_settings(settings)
    , m_ui(ui)
    , m_project(project)
{
}

void ModelConfigController::openSettings()
{
    if (m_settings->isSettingsVisible()) {
        m_settings->hideSettings();
        m_settings->setSettingsButtonText("LLM Settings");
        return;
    }

    m_settings->reloadSettingsFromConfig();
    m_settings->showSettings();
    m_settings->setSettingsButtonText("Hide LLM Settings");
}

void ModelConfigController::editBasePrompt()
{
    PromptManager* pm = static_cast<PromptManager*>(m_project->promptMgr());
    PromptEditorDialog d(pm, nullptr);
    d.exec();
}

void ModelConfigController::setupModelWithGenerationParams()
{
    AppConfig cfg;
    cfg.load();

    QVariantMap genParams;
    genParams["temperature"] = cfg.temperature;
    genParams["top_p"] = cfg.topP;
    genParams["top_k"] = cfg.topK;
    genParams["repeat_penalty"] = cfg.repeatPenalty;
    genParams["presence_penalty"] = cfg.presencePenalty;
    genParams["frequency_penalty"] = cfg.frequencyPenalty;
    genParams["num_predict"] = cfg.numPredict;
    genParams["num_ctx"] = cfg.numCtx;
    genParams["mirostat"] = cfg.mirostat;
    genParams["mirostat_tau"] = cfg.mirostatTau;
    genParams["mirostat_eta"] = cfg.mirostatEta;
    genParams["seed"] = cfg.seed;

    if (!cfg.stopWords.isEmpty()) {
        QJsonArray stopArray;
        for (const QString& word : cfg.stopWords) {
            stopArray.append(word);
        }
        genParams["stop"] = stopArray;
    }

    genParams["reasoning_effort"] = cfg.reasoningEffort;
    genParams["temperature_decay"] = cfg.temperatureDecay;
    genParams["repeat_last_n"] = cfg.repeatLastN;

    if (!m_model->selectedModel().isEmpty()) {
        m_model->configure(m_model->selectedModel(), genParams);
    }
}

void ModelConfigController::applySettingsFromDialog()
{
    const OllamaAdapter::Provider provider =
        OllamaAdapter::providerFromString(m_settings->providerString());
    const bool providerChanged = (m_model->provider() != static_cast<int>(provider));

    m_model->setProvider(static_cast<int>(provider));
    m_model->setAgentProvider(static_cast<int>(provider));

    QUrl endpoint(m_settings->endpointString().trimmed());
    if (!endpoint.isValid() || endpoint.toString().trimmed().isEmpty())
        endpoint = OllamaAdapter::defaultEndpointForProvider(provider);
    m_model->setEndpoint(endpoint);
    m_model->setAgentEndpoint(endpoint);

    const QVariantMap opts = m_settings->paramsMap();
    const QString selectedModel = m_model->selectedModel().trimmed();
    const QString selectedAgentModel = m_model->selectedAgentModel().trimmed();
    if (!selectedModel.isEmpty())
        m_model->configure(selectedModel, opts);
    if (!selectedAgentModel.isEmpty())
        m_model->configureAgentModel(selectedAgentModel, opts);

    AppConfig cfg;
    cfg.load();
    cfg.provider = OllamaAdapter::providerToString(provider);
    cfg.endpoint = endpoint;
    cfg.params = opts;
    if (providerChanged)
        cfg.selectedModel.clear();

    cfg.temperature = opts.value("temperature", 1).toDouble();
    cfg.topP = opts.value("top_p", 0.9).toDouble();
    cfg.topK = opts.value("top_k", 40).toInt();
    cfg.repeatPenalty = opts.value("repeat_penalty", 1.1).toDouble();
    cfg.presencePenalty = opts.value("presence_penalty", 0.0).toDouble();
    cfg.frequencyPenalty = opts.value("frequency_penalty", 0.0).toDouble();
    cfg.numPredict = opts.value("num_predict", -1).toInt();
    cfg.numCtx = opts.value("num_ctx", 4096).toInt();
    cfg.mirostat = opts.value("mirostat", 0).toInt();
    cfg.mirostatTau = opts.value("mirostat_tau", 5.0).toDouble();
    cfg.mirostatEta = opts.value("mirostat_eta", 0.1).toDouble();
    cfg.seed = opts.value("seed", 0).toULongLong();

    QStringList stopWords;
    if (opts.contains("stop")) {
        const QJsonArray stopArray = opts.value("stop").toJsonArray();
        for (const QJsonValue& val : stopArray) {
            const QString word = val.toString().trimmed();
            if (!word.isEmpty())
                stopWords << word;
        }
    }
    cfg.stopWords = stopWords;

    cfg.reasoningEffort = opts.value("reasoning_effort", "medium").toString();
    cfg.temperatureDecay = opts.value("temperature_decay", 0.0).toDouble();
    cfg.repeatLastN = opts.value("repeat_last_n", 64).toInt();
    cfg.save();

    m_model->refreshModelList();

    m_ui->statusBarMessage(QString("Provider: %1 | Model: %2")
                               .arg(m_model->providerName(),
                                    m_model->selectedModel().trimmed().isEmpty()
                                        ? QString("<not selected>")
                                        : m_model->selectedModel().trimmed()));
    m_ui->log(QString("LLM settings applied: provider=%1 endpoint=%2")
                .arg(m_model->providerName(), endpoint.toString()));
}
