#include "ChatDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>

ChatDialog::ChatDialog(OllamaAdapter* model, QWidget* parent):QWidget(parent),m_model(model){
    setWindowTitle(tr("Chat / Instruction"));
    resize(680,480);
    auto v = new QVBoxLayout(this);
    m_chat = new StyleTextEdit(this);
    m_chat->setReadOnly(true);
    m_cb_model = new MemoryComboBox("modelSelector", this);
    v->addWidget(m_chat,1);
    auto h = new QHBoxLayout();
    m_input = new InputTextEdit(this);
    m_send = new StyledButton("Отправить", this);
    m_stop = new StyledButton("Остановить", this);
    h->addWidget(m_cb_model);
    h->addWidget(m_input,1);
    h->addWidget(m_send);
    h->addWidget(m_stop);
    v->addLayout(h);
    connect(m_send, &StyledButton::clicked, this, &ChatDialog::onSend);
    connect(m_stop, &StyledButton::clicked, this, &ChatDialog::onStop);
    connect(m_input, &InputTextEdit::sendMessage, this, &ChatDialog::onSend);

    connect(m_cb_model, &QComboBox::currentTextChanged, this, [this](const QString &text){
        m_model->configure(text, QVariantMap());
    });

    pslot_updateModelsList();

    connect(m_input,  &InputTextEdit::signal_textEdited, this, [this](const QString &str){

        emit signal_updatelastUserMessage(str);
    });

}

void ChatDialog::onSend(){
    QString t = m_input->toPlainText().trimmed();
    if (t.isEmpty()) return;
    m_chat->append(QString("You: %1\n\n").arg(t));
    m_input->clear();
    emit sendMessage(t);
}

void ChatDialog::onStop(){
    if (m_model) m_model->stopStream();
}

void ChatDialog::pslot_updateModelsList(){
    //QString current_text = m_cb_model->currentText();
    //qDebug()<<"current_text = "<<current_text;
    m_cb_model->setItems(m_model->listModels());


}

void ChatDialog::appendAssistantChunk(const QString& s){
    static bool first = true;
    if (first) { m_chat->append("Assistant: \n"); first=false; }
    m_chat->moveCursor(QTextCursor::End);
    m_chat->insertPlainText(s);
}

void ChatDialog::finishAssistantMessage(){
    m_chat->append("\n<< stream finished >>");
    appendAssistantChunk("");
}

void ChatDialog::sendMessageToChat(const QString &text)
{
    emit sendMessage(text);
}

