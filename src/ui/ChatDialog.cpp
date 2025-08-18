#include "ChatDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

ChatDialog::ChatDialog(OllamaAdapter* model, QWidget* parent):QWidget(parent),m_model(model){
    setWindowTitle(tr("Chat / Instruction"));
    resize(680,480);
    auto v = new QVBoxLayout(this);
    m_chat = new QPlainTextEdit(this); m_chat->setReadOnly(true);
    m_cb_model = new QComboBox(this);
    v->addWidget(m_chat,1);
    auto h = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_send = new QPushButton("Send", this);
    m_stop = new QPushButton("Stop", this);
    h->addWidget(m_cb_model);
    h->addWidget(m_input,1);
    h->addWidget(m_send);
    h->addWidget(m_stop);
    v->addLayout(h);
    connect(m_send, &QPushButton::clicked, this, &ChatDialog::onSend);
    connect(m_stop, &QPushButton::clicked, this, &ChatDialog::onStop);

    connect(m_cb_model, &QComboBox::currentTextChanged, this, [this](const QString &text){
        m_model->configure(text, QVariantMap());
    });

    pslot_updateModelsList();
}

void ChatDialog::onSend(){
    QString t = m_input->text().trimmed();
    if (t.isEmpty()) return;
    m_chat->appendPlainText(QString("You: %1").arg(t));
    m_input->clear();
    emit sendMessage(t);
}

void ChatDialog::onStop(){
    if (m_model) m_model->stopStream();
}

void ChatDialog::pslot_updateModelsList()
{
    m_cb_model->clear();
    m_cb_model->addItems(m_model->listModels());
}

void ChatDialog::appendAssistantChunk(const QString& s){
    static bool first = true;
    if (first) { m_chat->appendPlainText("Assistant: "); first=false; }
    m_chat->moveCursor(QTextCursor::End);
    m_chat->insertPlainText(s);
}

void ChatDialog::finishAssistantMessage(){
    m_chat->appendPlainText("\n<< stream finished >>");
    appendAssistantChunk("");
}

void ChatDialog::sendMessageToChat(const QString &text)
{
    emit sendMessage(text);
}
