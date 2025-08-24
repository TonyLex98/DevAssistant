#include "OllamaSettingsDialog.h"
#include <QDialogButtonBox>

OllamaSettingsDialog::OllamaSettingsDialog(QWidget* parent):QDialog(parent){
    setWindowTitle("Ollama Settings");
    auto f = new QFormLayout(this);
    m_endpoint = new QLineEdit("http://127.0.0.1:11434", this);
    m_numCtx = new QSpinBox(this); m_numCtx->setRange(256, 256000); m_numCtx->setValue(64000);
    m_temp = new QDoubleSpinBox(this); m_temp->setRange(0.0, 5.0); m_temp->setSingleStep(1); m_temp->setValue(1);
    m_topP = new QDoubleSpinBox(this); m_topP->setRange(0.1, 1.0); m_topP->setSingleStep(0.05); m_topP->setValue(0.95);
    f->addRow("Endpoint:", m_endpoint);
    f->addRow("num_ctx:", m_numCtx);
    f->addRow("temperature:", m_temp);
    f->addRow("top_p:", m_topP);
    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
    f->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &OllamaSettingsDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &OllamaSettingsDialog::reject);
}

QVariantMap OllamaSettingsDialog::params() const{
    QVariantMap o;
    o["num_ctx"] = m_numCtx->value();
    o["temperature"] = m_temp->value();
    o["top_p"] = m_topP->value();
    return o;
}
