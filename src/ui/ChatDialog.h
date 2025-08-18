#pragma once
#include <QDialog>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include "../adapters/OllamaAdapter.h"
#include <QComboBox>

class ChatDialog : public QWidget {
    Q_OBJECT
public:
    explicit ChatDialog(OllamaAdapter* model, QWidget* parent=nullptr);

    void appendAssistantChunk(const QString& s);
    void finishAssistantMessage();

signals:
    void sendMessage(const QString& text);

private slots:
    void onSend();
    void onStop();
    void pslot_updateModelsList();

private:
    QPlainTextEdit* m_chat=nullptr;
    QLineEdit* m_input=nullptr;
    QPushButton* m_send=nullptr;
    QPushButton* m_stop=nullptr;
    OllamaAdapter* m_model=nullptr;
    QComboBox * m_cb_model;
};
