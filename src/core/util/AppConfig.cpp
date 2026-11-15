#include "core/util/AppConfig.h"
#include <QSettings>
#include <QCoreApplication>

const QVariantMap &AppConfig::getParams() const
{
    return params;
}

void AppConfig::setParams(const QVariantMap &newParams)
{
    params = newParams;
}

void AppConfig::load(){
    QSettings s;
    lastProjectPath = s.value("ui/lastProjectPath").toString();
    provider        = s.value("model/provider","ollama").toString().trimmed();
    if (provider.isEmpty())
        provider = QStringLiteral("ollama");
    selectedModel   = s.value("model/selected","").toString();
    const QString endpointDefault = (provider == QStringLiteral("llama.cpp"))
                                        ? QStringLiteral("http://127.0.0.1:8080")
                                        : QStringLiteral("http://127.0.0.1:11434");
    endpoint        = QUrl(s.value("model/endpoint", endpointDefault).toString());
    params          = s.value("model/params").toMap();
    
    // Generation parameters
    temperature = s.value("generation/temperature", 0.1).toDouble();
    topP = s.value("generation/top_p", 0.9).toDouble();
    topK = s.value("generation/top_k", 40).toInt();
    repeatPenalty = s.value("generation/repeat_penalty", 1.1).toDouble();
    presencePenalty = s.value("generation/presence_penalty", 0.0).toDouble();
    frequencyPenalty = s.value("generation/frequency_penalty", 0.0).toDouble();
    numPredict = s.value("generation/num_predict", -1).toInt();
    numCtx = s.value("generation/num_ctx", 4096).toInt();
    mirostat = s.value("generation/mirostat", 0).toInt();
    mirostatTau = s.value("generation/mirostat_tau", 5.0).toDouble();
    mirostatEta = s.value("generation/mirostat_eta", 0.1).toDouble();
    seed = s.value("generation/seed", 0).toULongLong();
    
    // Stop words
    QStringList stopList = s.value("generation/stop_words").toStringList();
    if (!stopList.isEmpty()) {
        stopWords = stopList;
    }
    
    reasoningEffort = s.value("generation/reasoning_effort", "medium").toString();
    temperatureDecay = s.value("generation/temperature_decay", 0.0).toDouble();
    repeatLastN = s.value("generation/repeat_last_n", 64).toInt();
}

void AppConfig::save() const{
    QSettings s;
    s.setValue("ui/lastProjectPath", lastProjectPath);
    s.setValue("model/provider", provider);
    s.setValue("model/selected", selectedModel);
    s.setValue("model/endpoint", endpoint.toString());
    s.setValue("model/params", params);
    
    // Generation parameters
    s.setValue("generation/temperature", temperature);
    s.setValue("generation/top_p", topP);
    s.setValue("generation/top_k", topK);
    s.setValue("generation/repeat_penalty", repeatPenalty);
    s.setValue("generation/presence_penalty", presencePenalty);
    s.setValue("generation/frequency_penalty", frequencyPenalty);
    s.setValue("generation/num_predict", numPredict);
    s.setValue("generation/num_ctx", numCtx);
    s.setValue("generation/mirostat", mirostat);
    s.setValue("generation/mirostat_tau", mirostatTau);
    s.setValue("generation/mirostat_eta", mirostatEta);
    s.setValue("generation/seed", seed);
    s.setValue("generation/stop_words", stopWords);
    s.setValue("generation/reasoning_effort", reasoningEffort);
    s.setValue("generation/temperature_decay", temperatureDecay);
    s.setValue("generation/repeat_last_n", repeatLastN);
}
