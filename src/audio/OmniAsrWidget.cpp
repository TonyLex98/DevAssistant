#include "OmniAsrWidget.h"
#include <QHBoxLayout>
#include <QMessageBox>

OmniAsrWidget::OmniAsrWidget(QWidget* parent) : QWidget(parent), m_isRecording(false) {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_recordBtn = new StyledButton("Record OmniASR", this);
    layout->addWidget(m_recordBtn);

    m_runner = new OmniAsrRunner(this);

    connect(m_recordBtn, &StyledButton::clicked, this, &OmniAsrWidget::onBtnClicked);
    connect(m_runner, &OmniAsrRunner::recognitionFinished, this, &OmniAsrWidget::onRecognitionFinished);
    connect(m_runner, &OmniAsrRunner::errorOccurred, this, &OmniAsrWidget::onError);
}

void OmniAsrWidget::onBtnClicked() {
    if (m_isRecording) {
        m_runner->stopRecording();
        m_recordBtn->setText("Processing...");
        m_recordBtn->setEnabled(false);
        m_isRecording = false;
    } else {
        m_runner->startRecording();
        m_recordBtn->setText("Stop Recording");
        m_isRecording = true;
    }
}

void OmniAsrWidget::onRecognitionFinished(const QString& text) {
    m_recordBtn->setText("Record OmniASR");
    m_recordBtn->setEnabled(true);
    emit textRecognized(text);
}

void OmniAsrWidget::onError(const QString& msg) {
    m_recordBtn->setText("Record OmniASR");
    m_recordBtn->setEnabled(true);
    m_isRecording = false;
    QMessageBox::warning(this, "ASR Error", msg);
}
