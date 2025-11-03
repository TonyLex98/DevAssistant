#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QTextEdit>

class OllamaSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit OllamaSettingsDialog(QWidget* parent=nullptr);
    QString endpoint() const { return m_endpoint->text(); }
    QVariantMap params() const;

private:
    QLineEdit* m_endpoint=nullptr;
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
    QLineEdit* m_reasoningEffort=nullptr;
    QDoubleSpinBox* m_temperatureDecay=nullptr;
    QSpinBox* m_repeatLastN=nullptr;
};
