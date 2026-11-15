#pragma once

#include <QComboBox>
#include <QWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTextEdit>

#include "ui/widgets/MemoryLineEdit.h"

class OllamaSettingsDialog : public QWidget
{
    Q_OBJECT
public:
    explicit OllamaSettingsDialog(QWidget* parent = nullptr);
    void reloadFromConfig();
    QString provider() const { return m_provider ? m_provider->currentData().toString() : QStringLiteral("ollama"); }
    QString endpoint() const { return m_endpoint->text(); }
    QVariantMap params() const;

signals:
    void saveRequested();

private:
    void applyDefaults();
    void loadFromConfig();

    QComboBox* m_provider = nullptr;
    MemoryLineEdit* m_endpoint = nullptr;

    QDoubleSpinBox* m_temperature = nullptr;
    QDoubleSpinBox* m_topP = nullptr;
    QSpinBox* m_topK = nullptr;
    QDoubleSpinBox* m_repeatPenalty = nullptr;
    QDoubleSpinBox* m_presencePenalty = nullptr;
    QDoubleSpinBox* m_frequencyPenalty = nullptr;
    QSpinBox* m_numPredict = nullptr;
    QSpinBox* m_numCtx = nullptr;
    QSpinBox* m_mirostat = nullptr;
    QDoubleSpinBox* m_mirostatTau = nullptr;
    QDoubleSpinBox* m_mirostatEta = nullptr;
    QSpinBox* m_seed = nullptr;
    QTextEdit* m_stopWords = nullptr;
    QComboBox* m_reasoningEffort = nullptr;
    QDoubleSpinBox* m_temperatureDecay = nullptr;
    QSpinBox* m_repeatLastN = nullptr;
};
