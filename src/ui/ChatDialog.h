#pragma once
#include <QDialog>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
#include "../adapters/OllamaAdapter.h"
#include <QComboBox>
#include <QKeyEvent>

class InputTextEdit : public QTextEdit {
    Q_OBJECT
public:
    explicit InputTextEdit(QWidget* parent = nullptr) : QTextEdit(parent) {}
signals:
    void sendMessage();
protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return && !(e->modifiers() & Qt::ShiftModifier)) {
            emit sendMessage();
        } else {
            QTextEdit::keyPressEvent(e);
        }
    }
};

class ChatDialog : public QWidget {
    Q_OBJECT
public:
    explicit ChatDialog(OllamaAdapter* model, QWidget* parent = nullptr);

    void appendAssistantChunk(const QString& s);
    void finishAssistantMessage();
    void sendMessageToChat(const QString& text);

signals:
    void sendMessage(const QString& text);

private slots:
    void onSend();
    void onStop();
    void pslot_updateModelsList();

private:
    QPlainTextEdit* m_chat = nullptr;
    InputTextEdit* m_input = nullptr;
    QPushButton* m_send = nullptr;
    QPushButton* m_stop = nullptr;
    OllamaAdapter* m_model = nullptr;
    QComboBox * m_cb_model;
};