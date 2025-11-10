#include "OllamaSettingsDialog.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QJsonArray>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QEventLoop>
#include "customWidgets/MemoryComboBox.h"

OllamaSettingsDialog::OllamaSettingsDialog(QWidget* parent):QDialog(parent){
    setWindowTitle("Ollama Settings");
    auto f = new QFormLayout(this);
    m_endpoint = new MemoryLineEdit("ollama_endpoint", this);
    m_endpoint->setText("http://127.0.0.1:11434");
    
    m_embeddingModel = new MemoryComboBox("ollama_embedding_model", this);
    m_embeddingModel->addItem("nomic-embed-text", "nomic-embed-text");
    m_embeddingModel->addItem("all-minilm", "all-minilm");
    m_embeddingModel->addItem("mxbai-embed-large", "mxbai-embed-large");

    // Основные параметры генерации
    m_temperature = new QDoubleSpinBox(this); m_temperature->setRange(0.0, 2.0); m_temperature->setSingleStep(0.1); m_temperature->setValue(0.1);
    m_topP = new QDoubleSpinBox(this); m_topP->setRange(0.0, 1.0); m_topP->setSingleStep(0.05); m_topP->setValue(0.9);
    m_topK = new QSpinBox(this); m_topK->setRange(0, 200); m_topK->setValue(40);
    m_repeatPenalty = new QDoubleSpinBox(this); m_repeatPenalty->setRange(0.0, 2.0); m_repeatPenalty->setSingleStep(0.1); m_repeatPenalty->setValue(1.1);
    m_presencePenalty = new QDoubleSpinBox(this); m_presencePenalty->setRange(-2.0, 2.0); m_presencePenalty->setSingleStep(0.1); m_presencePenalty->setValue(0.0);
    m_frequencyPenalty = new QDoubleSpinBox(this); m_frequencyPenalty->setRange(-2.0, 2.0); m_frequencyPenalty->setSingleStep(0.1); m_frequencyPenalty->setValue(0.0);
    m_numPredict = new QSpinBox(this); m_numPredict->setRange(-1, 10000); m_numPredict->setValue(-1); // -1 = без ограничения
    m_numCtx = new QSpinBox(this); m_numCtx->setRange(256, 128000); m_numCtx->setValue(64000); // Увеличили значение по умолчанию
    m_mirostat = new QSpinBox(this); m_mirostat->setRange(0, 2); m_mirostat->setValue(0);
    m_mirostatTau = new QDoubleSpinBox(this); m_mirostatTau->setRange(0.0, 10.0); m_mirostatTau->setSingleStep(0.1); m_mirostatTau->setValue(5.0);
    m_mirostatEta = new QDoubleSpinBox(this); m_mirostatEta->setRange(0.0, 1.0); m_mirostatEta->setSingleStep(0.01); m_mirostatEta->setValue(0.1);
    m_seed = new QSpinBox(this); m_seed->setRange(0, 1000000000); m_seed->setValue(0);
    m_stopWords = new QTextEdit(this); m_stopWords->setMaximumHeight(100); // Для ввода нескольких слов
    m_reasoningEffort = new MemoryLineEdit("ollama_reasoning_effort", this); m_reasoningEffort->setText("medium");
    m_temperatureDecay = new QDoubleSpinBox(this); m_temperatureDecay->setRange(0.0, 1.0); m_temperatureDecay->setSingleStep(0.01); m_temperatureDecay->setValue(0.0);
    m_repeatLastN = new QSpinBox(this); m_repeatLastN->setRange(0, 1000); m_repeatLastN->setValue(64);

    f->addRow("Endpoint:", m_endpoint);
    f->addRow("Embedding Model:", m_embeddingModel);

    // Группировка параметров для лучшей организации
    f->addRow(new QLabel("=== GENERATION PARAMETERS ==="));
    f->addRow("temperature:", m_temperature);
    f->addRow("top_p:", m_topP);
    f->addRow("top_k:", m_topK);
    f->addRow("repeat_penalty:", m_repeatPenalty);
    f->addRow("presence_penalty:", m_presencePenalty);
    f->addRow("frequency_penalty:", m_frequencyPenalty);
    f->addRow("num_predict:", m_numPredict);
    f->addRow("num_ctx:", m_numCtx);
    f->addRow("mirostat:", m_mirostat);
    f->addRow("mirostat_tau:", m_mirostatTau);
    f->addRow("mirostat_eta:", m_mirostatEta);
    f->addRow("seed:", m_seed);
    f->addRow("stop_words (one per line):", m_stopWords);
    f->addRow("reasoning_effort:", m_reasoningEffort);
    f->addRow("temperature_decay:", m_temperatureDecay);
    f->addRow("repeat_last_n:", m_repeatLastN);

    // Load available embedding models from Ollama
    QTimer::singleShot(0, this, &OllamaSettingsDialog::loadEmbeddingModels);

    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
    f->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &OllamaSettingsDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &OllamaSettingsDialog::reject);
}

void OllamaSettingsDialog::loadEmbeddingModels()
{
    QUrl url(m_endpoint->text());
    url.setPath("/api/tags");
    
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    QNetworkReply* reply = manager.get(request);
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(5000); // 5 seconds timeout
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeoutTimer.start();
    loop.exec();
    
    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        // Timeout occurred
        reply->abort();
        reply->deleteLater();
        return;
    }
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QJsonArray models = obj.value("models").toArray();
        
        // Clear existing items except default ones
        m_embeddingModel->clear();
        m_embeddingModel->addItem("nomic-embed-text", "nomic-embed-text");
        m_embeddingModel->addItem("all-minilm", "all-minilm");
        m_embeddingModel->addItem("mxbai-embed-large", "mxbai-embed-large");
        
        // Add all models that might be embedding models
        for (const QJsonValue& val : models) {
            QJsonObject modelObj = val.toObject();
            QString name = modelObj.value("name").toString();
            
            // Check if this looks like an embedding model by name
            if (name.contains("embed", Qt::CaseInsensitive) || 
                name.contains("minilm", Qt::CaseInsensitive) ||
                name.contains("all", Qt::CaseInsensitive) ||
                name.contains("bert", Qt::CaseInsensitive)) {
                
                // Only add if not already in the list
                bool exists = false;
                for (int i = 0; i < m_embeddingModel->count(); ++i) {
                    if (m_embeddingModel->itemData(i).toString() == name) {
                        exists = true;
                        break;
                    }
                }
                
                if (!exists) {
                    m_embeddingModel->addItem(name, name);
                }
            }
        }
    }
    
    reply->deleteLater();
}

QVariantMap OllamaSettingsDialog::params() const{
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

    // Добавляем stop-слова
    if (!m_stopWords->toPlainText().trimmed().isEmpty()) {
        QStringList stopWords = m_stopWords->toPlainText().split("\n", QString::SkipEmptyParts);
        QJsonArray stopArray;
        for (const QString& word : stopWords) {
            if (!word.trimmed().isEmpty()) {
                stopArray.append(word.trimmed());
            }
        }
        if (!stopArray.isEmpty()) {
            o["stop"] = stopArray;
        }
    }

    o["reasoning_effort"] = m_reasoningEffort->text();
    o["temperature_decay"] = m_temperatureDecay->value();
    o["repeat_last_n"] = m_repeatLastN->value();

    return o;
}