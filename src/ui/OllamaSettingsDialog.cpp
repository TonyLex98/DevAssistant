#include "OllamaSettingsDialog.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabWidget>
#include <QVBoxLayout>

#include "adapters/OllamaAdapter.h"
#include "utils/AppConfig.h"

OllamaSettingsDialog::OllamaSettingsDialog(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("llmSettingsWidget"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    setStyleSheet(
        "QWidget#llmSettingsWidget { background: #f5fbfa; border: 1px solid #bfeee7; border-radius: 8px; }"
        "QLabel#title { font: 700 18px 'Segoe UI'; color: #0f172a; }"
        "QLabel#subtitle { font: 10pt 'Segoe UI'; color: #334155; }"
        "QGroupBox {"
        "  border: 1px solid #bfeee7;"
        "  border-radius: 8px;"
        "  margin-top: 10px;"
        "  padding-top: 10px;"
        "  background: #ffffff;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 4px;"
        "  color: #0f766e;"
        "  font: 600 10pt 'Segoe UI';"
        "}"
        "QTabWidget::pane {"
        "  border: 1px solid #cdeee9;"
        "  border-radius: 8px;"
        "  background: #ffffff;"
        "}"
        "QTabBar::tab {"
        "  background: #def7f2;"
        "  border: 1px solid #bfeee7;"
        "  border-bottom: none;"
        "  padding: 6px 10px;"
        "  min-width: 70px;"
        "  color: #0f172a;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #ffffff;"
        "  color: #0f766e;"
        "}"
        "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTextEdit {"
        "  border: 1px solid #bfeee7;"
        "  border-radius: 6px;"
        "  padding: 6px 8px;"
        "  background: #ffffff;"
        "}"
        "QPushButton {"
        "  min-width: 72px;"
        "  padding: 6px 10px;"
        "  border-radius: 8px;"
        "}"
    );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    auto* title = new QLabel(tr("Generation and Connection"), this);
    title->setObjectName("title");
    root->addWidget(title);

    auto* subtitle = new QLabel(
        tr("Select provider (ollama/llama.cpp), endpoint and generation parameters. "
           "Changes are applied after pressing Save."),
        this);
    subtitle->setObjectName("subtitle");
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    auto* tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    m_provider = new QComboBox(this);
    m_provider->addItem(QStringLiteral("ollama"), QStringLiteral("ollama"));
    m_provider->addItem(QStringLiteral("llama.cpp"), QStringLiteral("llama.cpp"));

    m_endpoint = new MemoryLineEdit("ollama_endpoint", this);

    m_temperature = new QDoubleSpinBox(this);
    m_temperature->setRange(0.0, 2.0);
    m_temperature->setSingleStep(0.05);
    m_temperature->setDecimals(2);

    m_topP = new QDoubleSpinBox(this);
    m_topP->setRange(0.0, 1.0);
    m_topP->setSingleStep(0.05);
    m_topP->setDecimals(2);

    m_topK = new QSpinBox(this);
    m_topK->setRange(0, 200);

    m_repeatPenalty = new QDoubleSpinBox(this);
    m_repeatPenalty->setRange(0.0, 2.0);
    m_repeatPenalty->setSingleStep(0.05);
    m_repeatPenalty->setDecimals(2);

    m_presencePenalty = new QDoubleSpinBox(this);
    m_presencePenalty->setRange(-2.0, 2.0);
    m_presencePenalty->setSingleStep(0.05);
    m_presencePenalty->setDecimals(2);

    m_frequencyPenalty = new QDoubleSpinBox(this);
    m_frequencyPenalty->setRange(-2.0, 2.0);
    m_frequencyPenalty->setSingleStep(0.05);
    m_frequencyPenalty->setDecimals(2);

    m_numPredict = new QSpinBox(this);
    m_numPredict->setRange(-1, 10000);
    m_numPredict->setSpecialValueText(tr("No limit (-1)"));

    m_numCtx = new QSpinBox(this);
    m_numCtx->setRange(256, 128000);

    m_mirostat = new QSpinBox(this);
    m_mirostat->setRange(0, 2);

    m_mirostatTau = new QDoubleSpinBox(this);
    m_mirostatTau->setRange(0.0, 10.0);
    m_mirostatTau->setSingleStep(0.1);
    m_mirostatTau->setDecimals(2);

    m_mirostatEta = new QDoubleSpinBox(this);
    m_mirostatEta->setRange(0.0, 1.0);
    m_mirostatEta->setSingleStep(0.01);
    m_mirostatEta->setDecimals(2);

    m_seed = new QSpinBox(this);
    m_seed->setRange(0, 1000000000);

    m_stopWords = new QTextEdit(this);
    m_stopWords->setPlaceholderText(tr("One stop word per line"));
    m_stopWords->setMinimumHeight(110);

    m_reasoningEffort = new QComboBox(this);
    m_reasoningEffort->setEditable(true);
    m_reasoningEffort->addItems({QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high")});

    m_temperatureDecay = new QDoubleSpinBox(this);
    m_temperatureDecay->setRange(0.0, 1.0);
    m_temperatureDecay->setSingleStep(0.01);
    m_temperatureDecay->setDecimals(2);

    m_repeatLastN = new QSpinBox(this);
    m_repeatLastN->setRange(0, 4096);

    {
        auto* page = new QWidget(tabs);
        auto* lay = new QVBoxLayout(page);
        lay->setContentsMargins(12, 12, 12, 12);
        lay->setSpacing(10);

        auto* conn = new QGroupBox(tr("Connection"), page);
        auto* f = new QFormLayout(conn);
        f->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        f->addRow(tr("Provider"), m_provider);
        f->addRow(tr("Endpoint"), m_endpoint);

        auto* hint = new QLabel(
            tr("Defaults: ollama -> http://127.0.0.1:11434, llama.cpp -> http://127.0.0.1:8080"),
            page);
        hint->setWordWrap(true);
        hint->setObjectName("subtitle");

        lay->addWidget(conn);
        lay->addWidget(hint);
        lay->addStretch(1);
        tabs->addTab(page, tr("Connection"));
    }

    {
        auto* page = new QWidget(tabs);
        auto* lay = new QVBoxLayout(page);
        lay->setContentsMargins(12, 12, 12, 12);
        lay->setSpacing(10);

        auto* sampling = new QGroupBox(tr("Sampling"), page);
        auto* fs = new QFormLayout(sampling);
        fs->addRow(tr("Temperature"), m_temperature);
        fs->addRow(tr("Top P"), m_topP);
        fs->addRow(tr("Top K"), m_topK);

        auto* penalties = new QGroupBox(tr("Penalties and repetition"), page);
        auto* fp = new QFormLayout(penalties);
        fp->addRow(tr("Repeat penalty"), m_repeatPenalty);
        fp->addRow(tr("Presence penalty"), m_presencePenalty);
        fp->addRow(tr("Frequency penalty"), m_frequencyPenalty);
        fp->addRow(tr("Repeat last N"), m_repeatLastN);
        fp->addRow(tr("Temperature decay"), m_temperatureDecay);

        auto* miro = new QGroupBox(tr("Mirostat"), page);
        auto* fm = new QFormLayout(miro);
        fm->addRow(tr("Mode (0/1/2)"), m_mirostat);
        fm->addRow(tr("Tau"), m_mirostatTau);
        fm->addRow(tr("Eta"), m_mirostatEta);

        lay->addWidget(sampling);
        lay->addWidget(penalties);
        lay->addWidget(miro);
        lay->addStretch(1);
        tabs->addTab(page, tr("Generation"));
    }

    {
        auto* page = new QWidget(tabs);
        auto* lay = new QVBoxLayout(page);
        lay->setContentsMargins(12, 12, 12, 12);
        lay->setSpacing(10);

        auto* limits = new QGroupBox(tr("Limits"), page);
        auto* fl = new QFormLayout(limits);
        fl->addRow(tr("Num context (num_ctx)"), m_numCtx);
        fl->addRow(tr("Num predict (num_predict)"), m_numPredict);
        fl->addRow(tr("Seed"), m_seed);
        fl->addRow(tr("Reasoning effort"), m_reasoningEffort);

        auto* stop = new QGroupBox(tr("Stop words"), page);
        auto* stopLay = new QVBoxLayout(stop);
        stopLay->addWidget(m_stopWords);

        lay->addWidget(limits);
        lay->addWidget(stop);
        lay->addStretch(1);
        tabs->addTab(page, tr("Context and output"));
    }

    auto* buttonRow = new QHBoxLayout();
    auto* resetBtn = new QPushButton(tr("Reset"), this);
    auto* saveBtn = new QPushButton(tr("Save"), this);
    buttonRow->addStretch(1);
    buttonRow->addWidget(resetBtn);
    buttonRow->addWidget(saveBtn);
    root->addLayout(buttonRow);
    connect(resetBtn, &QPushButton::clicked, this, &OllamaSettingsDialog::applyDefaults);
    connect(saveBtn, &QPushButton::clicked, this, &OllamaSettingsDialog::saveRequested);

    connect(m_provider, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString endpointText = m_endpoint->text().trimmed();
        if (endpointText.isEmpty() ||
            endpointText == QStringLiteral("http://127.0.0.1:11434") ||
            endpointText == QStringLiteral("http://127.0.0.1:8080")) {
            if (m_provider->currentData().toString() == QStringLiteral("llama.cpp"))
                m_endpoint->setText(QStringLiteral("http://127.0.0.1:8080"));
            else
                m_endpoint->setText(QStringLiteral("http://127.0.0.1:11434"));
        }
    });

    applyDefaults();
    loadFromConfig();
}

void OllamaSettingsDialog::reloadFromConfig()
{
    loadFromConfig();
}

void OllamaSettingsDialog::applyDefaults()
{
    const int providerIdx = m_provider->findData(QStringLiteral("ollama"));
    if (providerIdx >= 0)
        m_provider->setCurrentIndex(providerIdx);

    m_endpoint->setText(QStringLiteral("http://127.0.0.1:11434"));
    m_temperature->setValue(0.10);
    m_topP->setValue(0.90);
    m_topK->setValue(40);
    m_repeatPenalty->setValue(1.10);
    m_presencePenalty->setValue(0.00);
    m_frequencyPenalty->setValue(0.00);
    m_numPredict->setValue(-1);
    m_numCtx->setValue(4096);
    m_mirostat->setValue(0);
    m_mirostatTau->setValue(5.00);
    m_mirostatEta->setValue(0.10);
    m_seed->setValue(0);
    m_temperatureDecay->setValue(0.00);
    m_repeatLastN->setValue(64);
    m_reasoningEffort->setCurrentText(QStringLiteral("medium"));
    m_stopWords->clear();
}

void OllamaSettingsDialog::loadFromConfig()
{
    AppConfig cfg;
    cfg.load();

    const QString provider = cfg.provider.trimmed().isEmpty() ? QStringLiteral("ollama") : cfg.provider.trimmed();
    int providerIdx = m_provider->findData(provider);
    if (providerIdx < 0)
        providerIdx = m_provider->findData(QStringLiteral("ollama"));
    if (providerIdx >= 0)
        m_provider->setCurrentIndex(providerIdx);

    if (cfg.endpoint.isValid() && !cfg.endpoint.toString().trimmed().isEmpty()) {
        m_endpoint->setText(cfg.endpoint.toString());
    } else {
        m_endpoint->setText(OllamaAdapter::defaultEndpointForProvider(
                                OllamaAdapter::providerFromString(provider)).toString());
    }

    m_temperature->setValue(cfg.temperature);
    m_topP->setValue(cfg.topP);
    m_topK->setValue(cfg.topK);
    m_repeatPenalty->setValue(cfg.repeatPenalty);
    m_presencePenalty->setValue(cfg.presencePenalty);
    m_frequencyPenalty->setValue(cfg.frequencyPenalty);
    m_numPredict->setValue(cfg.numPredict);
    m_numCtx->setValue(cfg.numCtx);
    m_mirostat->setValue(cfg.mirostat);
    m_mirostatTau->setValue(cfg.mirostatTau);
    m_mirostatEta->setValue(cfg.mirostatEta);
    m_seed->setValue(static_cast<int>(cfg.seed));
    m_temperatureDecay->setValue(cfg.temperatureDecay);
    m_repeatLastN->setValue(cfg.repeatLastN);

    QString effort = cfg.reasoningEffort.trimmed();
    if (effort.isEmpty())
        effort = QStringLiteral("medium");
    if (m_reasoningEffort->findText(effort) < 0)
        m_reasoningEffort->addItem(effort);
    m_reasoningEffort->setCurrentText(effort);

    if (!cfg.stopWords.isEmpty())
        m_stopWords->setPlainText(cfg.stopWords.join("\n"));
}

QVariantMap OllamaSettingsDialog::params() const
{
    QVariantMap o;
    o["temperature"] = m_temperature->value();
    o["top_p"] = m_topP->value();
    o["top_k"] = m_topK->value();
    o["repeat_penalty"] = m_repeatPenalty->value();
    o["presence_penalty"] = m_presencePenalty->value();
    o["frequency_penalty"] = m_frequencyPenalty->value();
    o["num_predict"] = m_numPredict->value();
    o["num_ctx"] = m_numCtx->value();
    o["mirostat"] = m_mirostat->value();
    o["mirostat_tau"] = m_mirostatTau->value();
    o["mirostat_eta"] = m_mirostatEta->value();
    o["seed"] = m_seed->value();

    const QStringList stopWords = m_stopWords->toPlainText().split('\n', QString::SkipEmptyParts);
    if (!stopWords.isEmpty()) {
        QJsonArray stopArray;
        for (const QString& word : stopWords) {
            const QString trimmed = word.trimmed();
            if (!trimmed.isEmpty())
                stopArray.append(trimmed);
        }
        if (!stopArray.isEmpty())
            o["stop"] = stopArray;
    }

    QString effort = m_reasoningEffort->currentText().trimmed();
    if (effort.isEmpty())
        effort = QStringLiteral("medium");
    o["reasoning_effort"] = effort;
    o["temperature_decay"] = m_temperatureDecay->value();
    o["repeat_last_n"] = m_repeatLastN->value();

    return o;
}
