#pragma once
#include <QDialog>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <StyledButton.h>
#include "../adapters/OllamaAdapter.h"
#include <QComboBox>
#include <QKeyEvent>
#include <InputTextEdit.h>
#include <MemoryComboBox.h>
#include <StyleTextEdit.h>

class ChatDialog : public QWidget {
    Q_OBJECT
public:
    explicit ChatDialog(OllamaAdapter* model, QWidget* parent = nullptr);

    void appendAssistantChunk(const QString& s);
    void finishAssistantMessage();
    void sendMessageToChat(const QString& text);

signals:
    void sendMessage(const QString& text);
    void signal_updatelastUserMessage(const QString& lastUserMessage);

private slots:
    void onSend();
    void onStop();
    void pslot_updateModelsList();

private:
    StyleTextEdit* m_chat = nullptr;
    InputTextEdit* m_input = nullptr;
    StyledButton* m_send = nullptr;
    StyledButton* m_stop = nullptr;
    OllamaAdapter* m_model = nullptr;
    MemoryComboBox * m_cb_model;
};
