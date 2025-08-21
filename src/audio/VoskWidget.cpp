
#include "VoskWidget.h"
#include "VoskRunner.h"
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

VoskWidget::VoskWidget(QWidget* parent) : QWidget(parent) {
    m_dllEdit = new QLineEdit(this);
    m_modelEdit = new QLineEdit(this);
    m_srSpin = new QSpinBox(this);
    m_srSpin->setRange(8000, 48000);
    m_srSpin->setValue(16000);
    m_wordsChk = new QCheckBox(tr("Words"), this);
    m_partialWordsChk = new QCheckBox(tr("Partial words"), this);

    // Prefill example paths (edit to your own)
    m_dllEdit->setText(QStringLiteral("C:/vosk/libvosk.dll"));
    m_modelEdit->setText(QStringLiteral("C:/vosk/vosk-model-ru-0.22"));

    m_startBtn = new QPushButton(tr("Start"), this);
    m_stopBtn  = new QPushButton(tr("Stop"), this);
    m_stopBtn->setEnabled(false);

    m_partialLbl = new QLabel(this);
    m_partialLbl->setTextFormat(Qt::PlainText);
    m_partialLbl->setWordWrap(true);

    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);

    auto form = new QFormLayout;
    form->addRow(tr("vosk.dll:"), m_dllEdit);
    form->addRow(tr("Model folder:"), m_modelEdit);
    form->addRow(tr("Sample rate:"), m_srSpin);
    form->addRow(m_wordsChk);
    form->addRow(m_partialWordsChk);

    auto btns = new QHBoxLayout;
    btns->addWidget(m_startBtn);
    btns->addWidget(m_stopBtn);
    btns->addStretch(1);

    auto lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addLayout(btns);
    lay->addWidget(new QLabel(tr("Partial:"), this));
    lay->addWidget(m_partialLbl);
    lay->addWidget(new QLabel(tr("Final results:"), this));
    lay->addWidget(m_log, 1);
    setLayout(lay);

    m_runner = new VoskRunner(this);
    connect(m_runner, &VoskRunner::partialText, this, &VoskWidget::onPartial);
    connect(m_runner, &VoskRunner::finalText,   this, &VoskWidget::onFinal);
    connect(m_runner, &VoskRunner::error,       this, &VoskWidget::onError);

    connect(m_startBtn, &QPushButton::clicked, this, &VoskWidget::onStart);
    connect(m_stopBtn,  &QPushButton::clicked, this, &VoskWidget::onStop);
}

void VoskWidget::onStart() {
    m_runner->setDllPath(m_dllEdit->text().trimmed());
    m_runner->setModelPath(m_modelEdit->text().trimmed());
    m_runner->setSampleRate(m_srSpin->value());
    m_runner->setWords(m_wordsChk->isChecked());
    m_runner->setPartialWords(m_partialWordsChk->isChecked());
    if (m_runner->start()) {
        m_startBtn->setEnabled(false);
        m_stopBtn->setEnabled(true);
        m_log->append(tr("Started."));
    } else {
        m_log->append(tr("Failed to start (check dll/model paths and audio format)."));
    }
}

void VoskWidget::onStop() {
    m_runner->stop();
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_log->append(tr("Stopped."));
}

void VoskWidget::onPartial(const QString& s) {
    m_partialLbl->setText(s);
}

void VoskWidget::onFinal(const QString& s) {
    if (!s.isEmpty()) m_log->append(QStringLiteral("» %1").arg(s));
}

void VoskWidget::onError(const QString& s) {
    m_log->append(QStringLiteral("[ERR] %1").arg(s));
}
