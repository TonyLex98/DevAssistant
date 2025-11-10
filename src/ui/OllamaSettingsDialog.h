#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QTextEdit>
#include <QComboBox>
#include "customWidgets/MemoryLineEdit.h"

class OllamaSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit OllamaSettingsDialog(QWidget* parent=nullptr);
    QString endpoint() const { return m_endpoint->text(); }
    QString embeddingModel() const { return m_embeddingModel->currentText(); }
    void setEmbeddingModel(const QString& model) { m_embeddingModel->setCurrentText(model); }
    QVariantMap params() const;

private slots:
    void loadEmbeddingModels();

private:
    MemoryLineEdit* m_endpoint=nullptr;
    QComboBox* m_embeddingModel=nullptr;  // Model for embeddings - now a combobox
    // Основные параметры генерации
    QDoubleSpinBox* m_temperature=nullptr;
    QDoubleSpinBox* m_topP=nullptr;
    QSpinBox* m_topK=nullptr;
    QDoubleSpinBox* m_repeatPenalty=nullptr;
    QDoubleSpinBox* m_presencePenalty=nullptr;
    QDoubleSpinBox* m_frequencyPenalty=nullptr;
    QSpinBox* m_numPredict=nullptr;
    QSpinBox* m_numCtx=nullptr;
    QSpinBox* m_mirostat=nullptr;
    QDoubleSpinBox* m_mirostatTau=nullptr;
    QDoubleSpinBox* m_mirostatEta=nullptr;
    QSpinBox* m_seed=nullptr;
    QTextEdit* m_stopWords=nullptr;  // Для ввода множества stop-слов
    MemoryLineEdit* m_reasoningEffort=nullptr;
    QDoubleSpinBox* m_temperatureDecay=nullptr;
    QSpinBox* m_repeatLastN=nullptr;
};
