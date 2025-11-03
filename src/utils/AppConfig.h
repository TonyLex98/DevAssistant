#pragma once
#include <QString>
#include <QVariantMap>
#include <QUrl>

class AppConfig {
public:
    QString lastProjectPath;
    QString selectedModel;
    QUrl    endpoint;
    QVariantMap params;
    // Параметры генерации для Ollama
    double temperature = 0.1;
    double topP = 0.9;
    int topK = 40;
    double repeatPenalty = 1.1;
    double presencePenalty = 0.0;
    double frequencyPenalty = 0.0;
    int numPredict = -1;
    int numCtx = 4096;
    int mirostat = 0;
    double mirostatTau = 5.0;
    double mirostatEta = 0.1;
    quint64 seed = 0;
    QStringList stopWords;
    QString reasoningEffort = "medium";
    double temperatureDecay = 0.0;
    int repeatLastN = 64;

    void load();
    void save() const;
    const QVariantMap &getParams() const;
    void setParams(const QVariantMap &newParams);
};
