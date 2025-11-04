#include "MicStreamWidget.h"
#include "StreamRunner.h"
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

MicStreamWidget::MicStreamWidget(QWidget* parent) : QWidget(parent) {
    m_exeEdit   = new MemoryLineEdit("micStreamExe", this);
    m_modelEdit = new MemoryLineEdit("micStreamModel", this);
    m_langEdit  = new MemoryLineEdit("micStreamLang", this);
    m_langEdit->setPlaceholderText("ru (leave empty for default/en)");

    // Prefill with requested folders/paths
    m_exeEdit->setText(QStringLiteral("C:/whisper-bin-x64/Release/whisper-stream.exe"));
    m_modelEdit->setText(QStringLiteral("C:/Users/user/Documents/DevAssistantQt_qt5_qmake/DevAss/src/external/whisper.cpp/models/ggml-small.bin"));
    m_langEdit->setText(QString()); // auto or set "ru"

    m_stepSpin = new QSpinBox(this);
    m_stepSpin->setRange(100, 20000);
    m_stepSpin->setSingleStep(100);
    m_stepSpin->setValue(2000);

    m_lenSpin = new QSpinBox(this);
    m_lenSpin->setRange(500, 60000);
    m_lenSpin->setSingleStep(500);
    m_lenSpin->setValue(8000);

    m_keepSpin = new QSpinBox(this);
    m_keepSpin->setRange(0, 10000);
    m_keepSpin->setSingleStep(100);
    m_keepSpin->setValue(400);

    m_thrSpin = new QSpinBox(this);
    m_thrSpin->setRange(1, 32);
    m_thrSpin->setValue(4);

    m_startBtn = new QPushButton(tr("Start"), this);
    m_stopBtn  = new QPushButton(tr("Stop"), this);
    m_stopBtn->setEnabled(false);

    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);

    auto form = new QFormLayout;
    form->addRow(tr("whisper-stream.exe:"), m_exeEdit);
    form->addRow(tr("Model (.bin):"),       m_modelEdit);
    form->addRow(tr("Language:"),           m_langEdit);
    form->addRow(tr("Step (ms):"),          m_stepSpin);
    form->addRow(tr("Length (ms):"),        m_lenSpin);
    form->addRow(tr("Keep (ms):"),          m_keepSpin);
    form->addRow(tr("Threads:"),            m_thrSpin);

    auto btns = new QHBoxLayout;
    btns->addWidget(m_startBtn);
    btns->addWidget(m_stopBtn);
    btns->addStretch(1);

    auto lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addLayout(btns);
    lay->addWidget(m_log, 1);
    setLayout(lay);

    m_runner = new StreamRunner(this);
    connect(m_runner, &StreamRunner::textReady, this, &MicStreamWidget::onText);
    connect(m_runner, &StreamRunner::finished,  this, &MicStreamWidget::onFinished);
    connect(m_runner, &StreamRunner::error,     this, &MicStreamWidget::onError);

    connect(m_startBtn, &QPushButton::clicked, this, &MicStreamWidget::onStart);
    connect(m_stopBtn,  &QPushButton::clicked, this, &MicStreamWidget::onStop);
}

void MicStreamWidget::onStart() {
    const QString exe   = m_exeEdit->text().trimmed();
    const QString model = m_modelEdit->text().trimmed();
    const QString lang  = m_langEdit->text().trimmed();
    const int step   = m_stepSpin->value();
    const int length = m_lenSpin->value();
    const int keep   = m_keepSpin->value();
    const int thr    = m_thrSpin->value();

    m_log->append(tr("Starting..."));
    m_runner->start(exe, model, lang, step, length, keep, thr, /*deviceId*/ -1);
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
}

void MicStreamWidget::onStop() {
    m_runner->stop();
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_log->append(tr("Stopped."));
}

void MicStreamWidget::onText(const QString& s) {
    // Append raw stdout from whisper-stream
    const QString line = s.trimmed();
    if (!line.isEmpty())
        m_log->append(line);
}

void MicStreamWidget::onFinished(int code, QProcess::ExitStatus) {
    m_log->append(tr("Process finished with code %1").arg(code));
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}

void MicStreamWidget::onError(const QString& msg) {
    m_log->append(QStringLiteral("[ERR] %1").arg(msg));
}
