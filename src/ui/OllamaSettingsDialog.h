#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>

class OllamaSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit OllamaSettingsDialog(QWidget* parent=nullptr);
    QString endpoint() const { return m_endpoint->text(); }
    QVariantMap params() const;

private:
    QLineEdit* m_endpoint=nullptr;
    QSpinBox* m_numCtx=nullptr;
    QDoubleSpinBox* m_temp=nullptr;
    QDoubleSpinBox* m_topP=nullptr;
};
