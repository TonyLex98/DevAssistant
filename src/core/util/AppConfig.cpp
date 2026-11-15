#include "core/util/AppConfig.h"
#include <QSettings>
#include <QCoreApplication>
#include <QJsonArray>

const QVariantMap &AppConfig::getParams() const
{
    return params;
}

void AppConfig::setParams(const QVariantMap &newParams)
{
    params = newParams;
}

QVariantMap AppConfig::generationParams() const
{
    if (!params.isEmpty())
        return params;

    QVariantMap p;
    p["temperature"] = temperature;
    p["top_p"] = topP;
    p["top_k"] = topK;
    p["repeat_penalty"] = repeatPenalty;
    p["presence_penalty"] = presencePenalty;
    p["frequency_penalty"] = frequencyPenalty;
    p["num_predict"] = numPredict;
    p["num_ctx"] = numCtx;
    p["mirostat"] = mirostat;
    p["mirostat_tau"] = mirostatTau;
    p["mirostat_eta"] = mirostatEta;
    p["seed"] = seed;
    if (!stopWords.isEmpty())
        p["stop"] = stopWords;
    p["reasoning_effort"] = reasoningEffort;
    p["temperature_decay"] = temperatureDecay;
    p["repeat_last_n"] = repeatLastN;
    return p;
}

void AppConfig::setGenerationParams(const QVariantMap& o)
{
    params = o;
    temperature = o.value("temperature", 1).toDouble();
    topP = o.value("top_p", 0.9).toDouble();
    topK = o.value("top_k", 40).toInt();
    repeatPenalty = o.value("repeat_penalty", 1.1).toDouble();
    presencePenalty = o.value("presence_penalty", 0.0).toDouble();
    frequencyPenalty = o.value("frequency_penalty", 0.0).toDouble();
    numPredict = o.value("num_predict", -1).toInt();
    numCtx = o.value("num_ctx", 4096).toInt();
    mirostat = o.value("mirostat", 0).toInt();
    mirostatTau = o.value("mirostat_tau", 5.0).toDouble();
    mirostatEta = o.value("mirostat_eta", 0.1).toDouble();
    seed = o.value("seed", 0).toULongLong();
    stopWords.clear();
    const QVariant stop = o.value("stop");
    const QStringList words = stop.canConvert<QJsonArray>() && stop.type() != QVariant::StringList
            ? QVariant(stop.toJsonArray().toVariantList()).toStringList()
            : stop.toStringList();
    for (const QString& w : words)
        if (!w.trimmed().isEmpty())
            stopWords << w.trimmed();
    reasoningEffort = o.value("reasoning_effort", "medium").toString();
    temperatureDecay = o.value("temperature_decay", 0.0).toDouble();
    repeatLastN = o.value("repeat_last_n", 64).toInt();
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
